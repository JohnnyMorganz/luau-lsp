#include "LSP/Workspace.hpp"

#include <iostream>
#include <memory>

#include "LSP/LanguageServer.hpp"
#include "Platform/LSPPlatform.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "glob/match.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/TimeTrace.h"

LUAU_FASTFLAG(LuauSolverV2)

const Luau::ModulePtr WorkspaceFolder::getModule(const Luau::ModuleName& moduleName, bool forAutocomplete) const
{
    if (FFlag::LuauSolverV2 || !forAutocomplete)
        return frontend.moduleResolver.getModule(moduleName);
    else
        return frontend.moduleResolverForAutocomplete.getModule(moduleName);
}

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

void WorkspaceFolder::clearDiagnosticsForFiles(const std::vector<lsp::DocumentUri>& uris) const
{
    if (!client->capabilities.textDocument || !client->capabilities.textDocument->diagnostic)
    {
        for (const auto& uri : uris)
            client->publishDiagnostics(lsp::PublishDiagnosticsParams{uri, std::nullopt, {}});
    }
    else if (client->workspaceDiagnosticsToken)
    {
        std::vector<lsp::WorkspaceDocumentDiagnosticReport> reports;
        reports.reserve(uris.size());
        for (const auto& uri : uris)
        {
            lsp::WorkspaceDocumentDiagnosticReport report;
            report.uri = uri;
            report.kind = lsp::DocumentDiagnosticReportKind::Full;
            reports.push_back(report);
        }
        lsp::WorkspaceDiagnosticReportPartialResult report{reports};
        client->sendProgress({client->workspaceDiagnosticsToken.value(), report});
    }
    else
    {
        client->refreshWorkspaceDiagnostics();
    }
}

void WorkspaceFolder::clearDiagnosticsForFile(const lsp::DocumentUri& uri)
{
    clearDiagnosticsForFiles({uri});
}

void WorkspaceFolder::onDidChangeWatchedFiles(const std::vector<lsp::FileEvent>& changes)
{
    client->sendTrace("workspace: processing " + std::to_string(changes.size()) + " watched files changes");

    auto config = client->getConfiguration(rootUri);

    std::vector<Luau::ModuleName> dirtyFiles;
    std::vector<Uri> deletedFiles;

    for (const auto& change : changes)
    {
        auto filePath = change.uri.fsPath();

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
                continue;
            }
            else if (isIgnoredFile(filePath, config))
            {
                continue;
            }

            // Index the workspace on changes
            if (config.index.enabled && isConfigured)
            {
                auto moduleName = fileResolver.getModuleName(change.uri);
                frontend.markDirty(moduleName, &dirtyFiles);
            }

            if (change.type == lsp::FileChangeType::Deleted)
                deletedFiles.push_back(change.uri);
        }
    }

    // Parse require graph for files if indexing enabled
    for (const auto& dirtyModule : dirtyFiles)
        frontend.parse(dirtyModule);

    // Clear the diagnostics for files in case it was not managed
    clearDiagnosticsForFiles(deletedFiles);
}

/// When using lexically_relative, if the root-dir does not match, it will return a default-constructed path.
/// Commonly, this doesn't match due to difference in casing. On Windows, we force all drive letters to be lowercase to
/// resolve this issue
static std::filesystem::path normaliseDriveLetter(const std::filesystem::path& path)
{
#ifdef _WIN32
    if (path.has_root_path())
    {
        auto root = path.root_path().generic_string();
        if (root.size() >= 1 && isupper(root[0]))
        {
            return std::filesystem::path(std::string(1, tolower(root[0])) + path.generic_string().substr(1));
        }
    }
#endif
    return path;
}

/// Whether the file has been marked as ignored by any of the ignored lists in the configuration
bool WorkspaceFolder::isIgnoredFile(const std::filesystem::path& path, const std::optional<ClientConfiguration>& givenConfig)
{
    // We want to test globs against a relative path to workspace, since that's what makes most sense
    auto relativeFsPath = normaliseDriveLetter(path).lexically_relative(normaliseDriveLetter(rootUri.fsPath()));
    if (relativeFsPath == std::filesystem::path())
        throw JsonRpcException(lsp::ErrorCode::InternalError, "isIgnoredFile failed: relative path is default-constructed when constructing " +
                                                                  path.string() + " against " + rootUri.fsPath().string());
    auto relativePathString = relativeFsPath.generic_string(); // HACK: we convert to generic string so we get '/' separators

    auto config = givenConfig ? *givenConfig : client->getConfiguration(rootUri);
    std::vector<std::string> patterns = config.ignoreGlobs; // TODO: extend further?
    for (auto& pattern : patterns)
    {
        if (glob::gitignore_glob_match(relativePathString, pattern))
        {
            return true;
        }
    }
    return false;
}

bool WorkspaceFolder::isIgnoredFileForAutoImports(const std::filesystem::path& path, const std::optional<ClientConfiguration>& givenConfig)
{
    // We want to test globs against a relative path to workspace, since that's what makes most sense
    auto relativeFsPath = normaliseDriveLetter(path).lexically_relative(normaliseDriveLetter(rootUri.fsPath()));
    if (relativeFsPath == std::filesystem::path())
        throw JsonRpcException(lsp::ErrorCode::InternalError, "isIgnoredFileForAutoImports failed: relative path is default-constructed");
    auto relativePathString = relativeFsPath.generic_string(); // HACK: we convert to generic string so we get '/' separators

    auto config = givenConfig ? *givenConfig : client->getConfiguration(rootUri);
    std::vector<std::string> patterns = config.completion.imports.ignoreGlobs;
    for (auto& pattern : patterns)
    {
        if (glob::gitignore_glob_match(relativePathString, pattern))
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
        if (std::filesystem::weakly_canonical(resolvePath(file)) == canonicalised)
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
Luau::CheckResult WorkspaceFolder::checkSimple(const Luau::ModuleName& moduleName)
{
    try
    {
        return frontend.check(
            moduleName, Luau::FrontendOptions{/* retainFullTypeGraphs: */ false, /* forAutocomplete: */ false, /* runLintChecks: */ true});
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
Luau::CheckResult WorkspaceFolder::checkStrict(const Luau::ModuleName& moduleName, bool forAutocomplete)
{
    if (FFlag::LuauSolverV2)
        forAutocomplete = false;

    // HACK: note that a previous call to `Frontend::check(moduleName, { retainTypeGraphs: false })`
    // and then a call `Frontend::check(moduleName, { retainTypeGraphs: true })` will NOT actually
    // retain the type graph if the module is not marked dirty.
    // We do a manual check and dirty marking to fix this
    auto module = getModule(moduleName, forAutocomplete);
    if (module && module->internalTypes.types.empty()) // If we didn't retain type graphs, then the internalTypes arena is empty
        frontend.markDirty(moduleName);

    return frontend.check(moduleName, Luau::FrontendOptions{/* retainFullTypeGraphs: */ true, forAutocomplete, /* runLintChecks: */ true});
}

void WorkspaceFolder::indexFiles(const ClientConfiguration& config)
{
    LUAU_TIMETRACE_SCOPE("WorkspaceFolder::indexFiles", "LSP");
    if (!config.index.enabled)
        return;

    if (isNullWorkspace())
        return;

    client->sendTrace("workspace: indexing all files");

    size_t indexCount = 0;

    for (std::filesystem::recursive_directory_iterator next(rootUri.fsPath(), std::filesystem::directory_options::skip_permission_denied), end;
         next != end; ++next)
    {
        if (indexCount >= config.index.maxFiles)
        {
            client->sendWindowMessage(lsp::MessageType::Warning, "The maximum workspace index limit (" + std::to_string(config.index.maxFiles) +
                                                                     ") has been hit. This may cause some language features to only work partially "
                                                                     "(Find All References, Rename). If necessary, consider increasing the limit");
            break;
        }

        try
        {
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
        catch (const std::filesystem::filesystem_error& e)
        {
            client->sendLogMessage(lsp::MessageType::Warning, std::string("failed to index file: ") + e.what());
        }
    }

    client->sendTrace("workspace: indexing all files COMPLETED");
}

static void clearDisabledGlobals(const ClientPtr client, const Luau::GlobalTypes& globalTypes, const std::vector<std::string>& disabledGlobals)
{
    const auto targetScope = globalTypes.globalScope;
    for (const auto& disabledGlobal : disabledGlobals)
    {
        std::string library = disabledGlobal;
        std::optional<std::string> method = std::nullopt;

        if (const auto separator = disabledGlobal.find('.'); separator != std::string::npos)
        {
            library = disabledGlobal.substr(0, separator);
            method = disabledGlobal.substr(separator + 1);
        }

        const auto globalName = globalTypes.globalNames.names->get(library.c_str());
        if (globalName.value == nullptr)
        {
            client->sendLogMessage(lsp::MessageType::Warning, "disabling globals: skipping unknown global - " + disabledGlobal);
            continue;
        }

        if (auto binding = targetScope->bindings.find(globalName); binding != targetScope->bindings.end())
        {
            if (method)
            {
                const auto typeId = Luau::follow(binding->second.typeId);
                if (const auto ttv = Luau::getMutable<Luau::TableType>(typeId))
                {
                    if (contains(ttv->props, *method))
                    {
                        client->sendLogMessage(lsp::MessageType::Info, "disabling globals: erasing global - " + disabledGlobal);
                        ttv->props.erase(*method);
                    }
                    else
                        client->sendLogMessage(lsp::MessageType::Warning, "disabling globals: could not find method - " + disabledGlobal);
                }
                else if (const auto ctv = Luau::getMutable<Luau::ClassType>(typeId))
                {
                    if (contains(ctv->props, *method))
                    {
                        client->sendLogMessage(lsp::MessageType::Info, "disabling globals: erasing global - " + disabledGlobal);
                        ttv->props.erase(*method);
                    }
                    else
                        client->sendLogMessage(lsp::MessageType::Warning, "disabling globals: could not find method - " + disabledGlobal);
                }
                else
                {
                    client->sendLogMessage(lsp::MessageType::Warning,
                        "disabling globals: cannot clear method from global, only tables or classes are supported - " + disabledGlobal);
                }
            }
            else
            {
                client->sendLogMessage(lsp::MessageType::Info, "disabling globals: erasing global - " + disabledGlobal);
                targetScope->bindings.erase(globalName);
            }
        }
        else
        {
            client->sendLogMessage(lsp::MessageType::Warning, "disabling globals: skipping unknown global - " + disabledGlobal);
        }
    }
}

void WorkspaceFolder::registerTypes(const std::vector<std::string>& disabledGlobals)
{
    LUAU_TIMETRACE_SCOPE("WorkspaceFolder::initialize", "LSP");
    client->sendTrace("workspace initialization: registering Luau globals");
    Luau::registerBuiltinGlobals(frontend, frontend.globals);
    if (!FFlag::LuauSolverV2)
        Luau::registerBuiltinGlobals(frontend, frontend.globalsForAutocomplete);

    auto& tagRegisterGlobals = FFlag::LuauSolverV2 ? frontend.globals : frontend.globalsForAutocomplete;
    Luau::attachTag(Luau::getGlobalBinding(tagRegisterGlobals, "require"), "Require");

    if (client->definitionsFiles.empty())
        client->sendLogMessage(lsp::MessageType::Warning, "No definitions file provided by client");

    for (const auto& definitionsFile : client->definitionsFiles)
    {
        auto resolvedFilePath = resolvePath(definitionsFile);
        client->sendLogMessage(lsp::MessageType::Info, "Loading definitions file: " + resolvedFilePath.generic_string());

        auto definitionsContents = readFile(resolvedFilePath);
        if (!definitionsContents)
        {
            client->sendWindowMessage(lsp::MessageType::Error,
                "Failed to read definitions file " + resolvedFilePath.generic_string() + ". Extended types will not be provided");
            continue;
        }

        // Parse definitions file metadata
        client->sendTrace("workspace initialization: parsing definitions file metadata");
        auto metadata = types::parseDefinitionsFileMetadata(*definitionsContents);
        if (!definitionsFileMetadata)
            definitionsFileMetadata = metadata;
        client->sendTrace("workspace initialization: parsing definitions file metadata COMPLETED", json(definitionsFileMetadata).dump());

        client->sendTrace("workspace initialization: registering types definition");
        auto result = types::registerDefinitions(frontend, frontend.globals, *definitionsContents);
        if (!FFlag::LuauSolverV2)
            types::registerDefinitions(frontend, frontend.globalsForAutocomplete, *definitionsContents);
        client->sendTrace("workspace initialization: registering types definition COMPLETED");

        client->sendTrace("workspace: applying platform mutations on definitions");
        platform->mutateRegisteredDefinitions(frontend.globals, metadata);
        platform->mutateRegisteredDefinitions(frontend.globalsForAutocomplete, metadata);

        auto uri = Uri::file(resolvedFilePath);

        if (result.success)
        {
            // Clear any set diagnostics
            client->publishDiagnostics({uri, std::nullopt, {}});
        }
        else
        {
            client->sendWindowMessage(lsp::MessageType::Error,
                "Failed to read definitions file " + resolvedFilePath.generic_string() + ". Extended types will not be provided");

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

    if (!disabledGlobals.empty())
    {
        client->sendTrace("workspace initialization: removing disabled globals");
        clearDisabledGlobals(client, frontend.globals, disabledGlobals);
        if (!FFlag::LuauSolverV2)
            clearDisabledGlobals(client, frontend.globalsForAutocomplete, disabledGlobals);
        client->sendTrace("workspace initialization: removing disabled globals COMPLETED");
    }

    Luau::freeze(frontend.globals.globalTypes);
    if (!FFlag::LuauSolverV2)
        Luau::freeze(frontend.globalsForAutocomplete.globalTypes);
}

void WorkspaceFolder::setupWithConfiguration(const ClientConfiguration& configuration)
{
    LUAU_TIMETRACE_SCOPE("WorkspaceFolder::setupWithConfiguration", "LSP");
    client->sendTrace("workspace: setting up with configuration");

    // Apply first-time configuration
    if (!isConfigured)
    {
        isConfigured = true;

        client->sendTrace("workspace: first time configuration, setting appropriate platform");
        platform = LSPPlatform::getPlatform(configuration, &fileResolver, this);
        fileResolver.platform = platform.get();
        fileResolver.requireSuggester = fileResolver.platform->getRequireSuggester();

        registerTypes(configuration.types.disabledGlobals);
    }

    client->sendTrace("workspace: apply platform-specific configuration");

    platform->setupWithConfiguration(configuration);

    if (configuration.index.enabled)
        indexFiles(configuration);

    client->sendTrace("workspace: setting up with configuration COMPLETED");
}
