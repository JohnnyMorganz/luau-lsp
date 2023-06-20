#include "LSP/Workspace.hpp"

#include <iostream>
#include <climits>

#include "glob/glob.hpp"
#include "Luau/BuiltinDefinitions.h"
#include "LSP/LuauExt.hpp"

void WorkspaceFolder::openTextDocument(const lsp::DocumentUri& uri, const lsp::DidOpenTextDocumentParams& params)
{
    auto normalisedUri = fileResolver.normalisedUriString(uri);

    fileResolver.managedFiles.emplace(
        std::make_pair(normalisedUri, TextDocument(uri, params.textDocument.languageId, params.textDocument.version, params.textDocument.text)));

    // Mark the file as dirty as we don't know what changes were made to it
    auto moduleName = fileResolver.getModuleName(uri);
    frontend.markDirty(moduleName);
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

/// Whether the file has been marked as ignored by any of the ignored lists in the configuration
bool WorkspaceFolder::isIgnoredFile(const std::filesystem::path& path, const std::optional<ClientConfiguration>& givenConfig)
{
    // We want to test globs against a relative path to workspace, since thats what makes most sense
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
Luau::CheckResult WorkspaceFolder::checkSimple(const Luau::ModuleName& moduleName, bool runLintChecks)
{
    return frontend.check(moduleName, Luau::FrontendOptions{/* retainFullTypeGraphs: */ false, /* forAutocomplete: */ false, runLintChecks});
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

                // Check the module, discard the type graph
                // Since we don't care about types, we don't need to do it for autocomplete
                // TODO: can we get rid of the check, and only parse the require graph?
                checkSimple(moduleName);

                indexCount += 1;
            }
        }
    }
}

bool WorkspaceFolder::updateSourceMap()
{
    auto sourcemapPath = rootUri.fsPath() / "sourcemap.json";
    client->sendTrace("Updating sourcemap contents from " + sourcemapPath.generic_string());

    // Read in the sourcemap
    // TODO: we assume a sourcemap.json file in the workspace root
    if (auto sourceMapContents = readFile(sourcemapPath))
    {
        frontend.clear();
        fileResolver.updateSourceMap(sourceMapContents.value());

        // Recreate instance types
        auto config = client->getConfiguration(rootUri);
        instanceTypes.clear();
        // NOTE: expressive types is always enabled for autocomplete, regardless of the setting!
        // We pass the same setting even when we are registering autocomplete globals since
        // the setting impacts what happens to diagnostics (as both calls overwrite frontend.prepareModuleScope)
        types::registerInstanceTypes(frontend, frontend.globals, instanceTypes, fileResolver,
            /* expressiveTypes: */ config.diagnostics.strictDatamodelTypes);
        types::registerInstanceTypes(frontend, frontend.globalsForAutocomplete, instanceTypes, fileResolver,
            /* expressiveTypes: */ config.diagnostics.strictDatamodelTypes);

        return true;
    }
    else
    {
        return false;
    }
}

void WorkspaceFolder::initialize()
{
    Luau::registerBuiltinGlobals(frontend, frontend.globals, /* typeCheckForAutocomplete = */ false);
    Luau::registerBuiltinGlobals(frontend, frontend.globalsForAutocomplete, /* typeCheckForAutocomplete = */ true);

    Luau::attachTag(Luau::getGlobalBinding(frontend.globalsForAutocomplete, "require"), "Require");

    if (client->definitionsFiles.empty())
    {
        client->sendLogMessage(lsp::MessageType::Warning, "No definitions file provided by client");
    }

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

        auto result = types::registerDefinitions(frontend, frontend.globals, *definitionsContents, /* typeCheckForAutocomplete = */ false);
        types::registerDefinitions(frontend, frontend.globalsForAutocomplete, *definitionsContents, /* typeCheckForAutocomplete = */ true);

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
    isConfigured = true;
    if (configuration.sourcemap.enabled)
    {
        if (!isNullWorkspace() && !updateSourceMap())
        {
            client->sendWindowMessage(
                lsp::MessageType::Error, "Failed to load sourcemap.json for workspace '" + name + "'. Instance information will not be available");
        }
    }

    if (configuration.index.enabled)
        indexFiles(configuration);
}