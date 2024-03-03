#include "LSP/Workspace.hpp"

#include <iostream>
#include <memory>

#include "LSP/LanguageServer.hpp"
#include "Platform/LSPPlatform.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "glob/glob.hpp"
#include "Luau/BuiltinDefinitions.h"

LUAU_FASTFLAG(LuauStacklessTypeClone3)

void WorkspaceFolder::openTextDocument(const lsp::DocumentUri& uri, const lsp::DidOpenTextDocumentParams& params)
{
    auto normalisedUri = fileResolver.normalisedUriString(uri);

    fileResolver.managedFiles.emplace(
        std::make_pair(normalisedUri, TextDocument(uri, params.textDocument.languageId, params.textDocument.version, params.textDocument.text)));

    if (isConfigured)
    {
        // Mark the file as dirty as we don't know what changes were made to it
        auto moduleName = fileResolver.getModuleName(uri);
        frontend.markDirty(moduleName);
    }
}

void WorkspaceFolder::updateTextDocument(
    const lsp::DocumentUri& uri, const lsp::DidChangeTextDocumentParams& params, std::vector<Luau::ModuleName>* markedDirty)
{
    auto normalisedUri = fileResolver.normalisedUriString(uri);

    if (!contains(fileResolver.managedFiles, normalisedUri))
    {
        client->sendLogMessage(lsp::MessageType::Error, "Text Document not loaded locally: " + uri.toString());
        return;
    }
    auto& textDocument = fileResolver.managedFiles.at(normalisedUri);
    textDocument.update(params.contentChanges, params.textDocument.version);

    // Mark the module dirty for the typechecker
    auto moduleName = fileResolver.getModuleName(uri);
    frontend.markDirty(moduleName, markedDirty);
}

void WorkspaceFolder::closeTextDocument(const lsp::DocumentUri& uri)
{
    fileResolver.managedFiles.erase(fileResolver.normalisedUriString(uri));

    // Mark the module as dirty as we no longer track its changes
    auto config = client->getConfiguration(rootUri);
    auto moduleName = fileResolver.getModuleName(uri);
    frontend.markDirty(moduleName);

    // Refresh workspace diagnostics to clear diagnostics on ignored files
    if (!config.diagnostics.workspace || isIgnoredFile(uri.fsPath()))
        clearDiagnosticsForFile(uri);
}

void WorkspaceFolder::clearDiagnosticsForFile(const lsp::DocumentUri& uri)
{
    if (!client->capabilities.textDocument || !client->capabilities.textDocument->diagnostic)
    {
        client->publishDiagnostics(lsp::PublishDiagnosticsParams{uri, std::nullopt, {}});
    }
    else if (client->workspaceDiagnosticsToken)
    {
        lsp::WorkspaceDocumentDiagnosticReport documentReport;
        documentReport.uri = uri;
        documentReport.kind = lsp::DocumentDiagnosticReportKind::Full;
        lsp::WorkspaceDiagnosticReportPartialResult report{{documentReport}};
        client->sendProgress({client->workspaceDiagnosticsToken.value(), report});
    }
    else
    {
        client->refreshWorkspaceDiagnostics();
    }
}

void WorkspaceFolder::onDidChangeWatchedFiles(const lsp::FileEvent& change)
{
    auto filePath = change.uri.fsPath();
    auto config = client->getConfiguration(rootUri);

    platform->onDidChangeWatchedFiles(change);

    if (filePath.filename() == ".luaurc")
    {
        client->sendLogMessage(lsp::MessageType::Info, "Acknowledge config changed for workspace " + name + ", clearing configuration cache");
        fileResolver.clearConfigCache();

        // Recompute diagnostics
        recomputeDiagnostics(config);
    }
    else if (filePath.extension() == ".lua" || filePath.extension() == ".luau")
    {
        // Notify if it was a definitions file
        if (isDefinitionFile(filePath, config))
        {
            client->sendWindowMessage(
                lsp::MessageType::Info, "Detected changes to global definitions files. Please reload your workspace for this to take effect");
            return;
        }

        // Index the workspace on changes
        // We only update the require graph. We do not perform type checking
        if (config.index.enabled && isConfigured)
        {
            auto moduleName = fileResolver.getModuleName(change.uri);

            std::vector<Luau::ModuleName> markedDirty{};
            frontend.markDirty(moduleName, &markedDirty);

            if (change.type == lsp::FileChangeType::Created)
                frontend.parse(moduleName);

            // Re-check the reverse dependencies
            for (const auto& reverseDep : markedDirty)
                frontend.parse(reverseDep);
        }

        // Clear the diagnostics for the file in case it was not managed
        if (change.type == lsp::FileChangeType::Deleted)
            clearDiagnosticsForFile(change.uri);
    }
}

/// Whether the file has been marked as ignored by any of the ignored lists in the configuration
bool WorkspaceFolder::isIgnoredFile(const std::filesystem::path& path, const std::optional<ClientConfiguration>& givenConfig)
{
    // We want to test globs against a relative path to workspace, since that's what makes most sense
    auto relativePath = path.lexically_relative(rootUri.fsPath()).generic_string(); // HACK: we convert to generic string so we get '/' separators

    auto config = givenConfig ? *givenConfig : client->getConfiguration(rootUri);
    std::vector<std::string> patterns = config.ignoreGlobs; // TODO: extend further?
    for (auto& pattern : patterns)
    {
        if (glob::fnmatch_case(relativePath, pattern))
        {
            return true;
        }
    }
    return false;
}

bool WorkspaceFolder::isDefinitionFile(const std::filesystem::path& path, const std::optional<ClientConfiguration>& givenConfig)
{
    auto config = givenConfig ? *givenConfig : client->getConfiguration(rootUri);
    auto canonicalised = std::filesystem::weakly_canonical(path);

    for (auto& file : config.types.definitionFiles)
    {
        if (std::filesystem::weakly_canonical(file) == canonicalised)
        {
            return true;
        }
    }

    return false;
}

// Runs `Frontend::check` on the module and DISCARDS THE TYPE GRAPH.
// Uses the diagnostic type checker, so strictness and DM awareness is not enforced
// NOTE: do NOT use this if you later retrieve a ModulePtr (via frontend.moduleResolver.getModule). Instead use `checkStrict`
// NOTE: use `frontend.parse` if you do not care about typechecking
Luau::CheckResult WorkspaceFolder::checkSimple(const Luau::ModuleName& moduleName, bool runLintChecks)
{
    // TODO: We do not need to store the type graphs. But it leads to a bad bug if we disable it so for now, we keep the type graphs
    // https://github.com/Roblox/luau/issues/975
    if (!FFlag::LuauStacklessTypeClone3)
        return frontend.check(
            moduleName, Luau::FrontendOptions{/* retainFullTypeGraphs: */ true, /* forAutocomplete: */ false, /* runLintChecks: */ runLintChecks});

    try
    {
        return frontend.check(moduleName, Luau::FrontendOptions{/* retainFullTypeGraphs: */ false, /* forAutocomplete: */ false, runLintChecks});
    }
    catch (Luau::InternalCompilerError& err)
    {
        // TODO: RecursionLimitException is leaking out of frontend.check
        // https://github.com/Roblox/luau/issues/975
        // Remove this try-catch block once the above issue is fixed
        client->sendLogMessage(lsp::MessageType::Warning, "Luau InternalCompilerError caught in " + moduleName + ": " + err.what());
        return Luau::CheckResult{};
    }
}

// Runs `Frontend::check` on the module whilst retaining the type graph.
// Uses the autocomplete typechecker to enforce strictness and DM awareness.
// NOTE: a disadvantage of the autocomplete typechecker is that it has a timeout restriction that
// can often be hit
void WorkspaceFolder::checkStrict(const Luau::ModuleName& moduleName, bool forAutocomplete)
{
    // HACK: note that a previous call to `Frontend::check(moduleName, { retainTypeGraphs: false })`
    // and then a call `Frontend::check(moduleName, { retainTypeGraphs: true })` will NOT actually
    // retain the type graph if the module is not marked dirty.
    // We do a manual check and dirty marking to fix this
    auto module = forAutocomplete ? frontend.moduleResolverForAutocomplete.getModule(moduleName) : frontend.moduleResolver.getModule(moduleName);
    if (module && module->internalTypes.types.empty()) // If we didn't retain type graphs, then the internalTypes arena is empty
        frontend.markDirty(moduleName);

    frontend.check(moduleName, Luau::FrontendOptions{/* retainFullTypeGraphs: */ true, forAutocomplete, /* runLintChecks: */ false});
}

void WorkspaceFolder::indexFiles(const ClientConfiguration& config)
{
    if (!config.index.enabled)
        return;

    if (isNullWorkspace())
        return;

    client->sendTrace("workspace: indexing all files");

    size_t indexCount = 0;

    for (std::filesystem::recursive_directory_iterator next(rootUri.fsPath()), end; next != end; ++next)
    {
        if (indexCount >= config.index.maxFiles)
        {
            client->sendWindowMessage(lsp::MessageType::Warning, "The maximum workspace index limit (" + std::to_string(config.index.maxFiles) +
                                                                     ") has been hit. This may cause some language features to only work partially "
                                                                     "(Find All References, Rename). If necessary, consider increasing the limit");
            break;
        }

        if (next->is_regular_file() && next->path().has_extension() && !isDefinitionFile(next->path(), config) &&
            !isIgnoredFile(next->path(), config))
        {
            auto ext = next->path().extension();
            if (ext == ".lua" || ext == ".luau")
            {
                auto moduleName = fileResolver.getModuleName(Uri::file(next->path()));

                // Parse the module to infer require data
                // We do not perform any type checking here
                frontend.parse(moduleName);

                indexCount += 1;
            }
        }
    }

    client->sendTrace("workspace: indexing all files COMPLETED");
}

void WorkspaceFolder::initialize()
{
    client->sendTrace("workspace initialization: registering Luau globals");
    Luau::registerBuiltinGlobals(frontend, frontend.globals, /* typeCheckForAutocomplete = */ false);
    Luau::registerBuiltinGlobals(frontend, frontend.globalsForAutocomplete, /* typeCheckForAutocomplete = */ true);

    Luau::attachTag(Luau::getGlobalBinding(frontend.globalsForAutocomplete, "require"), "Require");

    if (client->definitionsFiles.empty())
        client->sendLogMessage(lsp::MessageType::Warning, "No definitions file provided by client");

    for (const auto& definitionsFile : client->definitionsFiles)
    {
        client->sendLogMessage(lsp::MessageType::Info, "Loading definitions file: " + definitionsFile.generic_string());

        auto definitionsContents = readFile(definitionsFile);
        if (!definitionsContents)
        {
            client->sendWindowMessage(lsp::MessageType::Error,
                "Failed to read definitions file " + definitionsFile.generic_string() + ". Extended types will not be provided");
            continue;
        }

        // Parse definitions file metadata
        client->sendTrace("workspace initialization: parsing definitions file metadata");
        if (auto metadata = types::parseDefinitionsFileMetadata(*definitionsContents))
            definitionsFileMetadata = metadata;
        client->sendTrace("workspace initialization: parsing definitions file metadata COMPLETED", json(definitionsFileMetadata).dump());

        client->sendTrace("workspace initialization: registering types definition");
        auto result = types::registerDefinitions(frontend, frontend.globals, *definitionsContents, /* typeCheckForAutocomplete = */ false);
        types::registerDefinitions(frontend, frontend.globalsForAutocomplete, *definitionsContents, /* typeCheckForAutocomplete = */ true);
        client->sendTrace("workspace initialization: registering types definition COMPLETED");

        auto uri = Uri::file(definitionsFile);

        if (result.success)
        {
            // Clear any set diagnostics
            client->publishDiagnostics({uri, std::nullopt, {}});
        }
        else
        {
            client->sendWindowMessage(lsp::MessageType::Error,
                "Failed to read definitions file " + definitionsFile.generic_string() + ". Extended types will not be provided");

            // Display relevant diagnostics
            std::vector<lsp::Diagnostic> diagnostics;
            for (auto& error : result.parseResult.errors)
                diagnostics.emplace_back(createParseErrorDiagnostic(error));

            if (result.module)
                for (auto& error : result.module->errors)
                    diagnostics.emplace_back(createTypeErrorDiagnostic(error, &fileResolver));

            client->publishDiagnostics({uri, std::nullopt, diagnostics});
        }
    }
    Luau::freeze(frontend.globals.globalTypes);
    Luau::freeze(frontend.globalsForAutocomplete.globalTypes);
}

void WorkspaceFolder::setupWithConfiguration(const ClientConfiguration& configuration)
{
    client->sendTrace("workspace: setting up with configuration");
    platform = LSPPlatform::getPlatform(configuration, &fileResolver, this);

    fileResolver.platform = platform.get();

    if (!isConfigured)
    {
        isConfigured = true;

        platform->mutateRegisteredDefinitions(frontend.globals, definitionsFileMetadata);
        platform->mutateRegisteredDefinitions(frontend.globalsForAutocomplete, definitionsFileMetadata);
    }

    if (configuration.index.enabled)
        indexFiles(configuration);

    platform->setupWithConfiguration(configuration);
}
