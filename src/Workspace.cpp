#include <iostream>
#include <limits.h>
#include "LSP/Workspace.hpp"

void WorkspaceFolder::openTextDocument(const lsp::DocumentUri& uri, const lsp::DidOpenTextDocumentParams& params)
{
    auto moduleName = fileResolver.getModuleName(uri);
    fileResolver.managedFiles.emplace(
        std::make_pair(moduleName, TextDocument(uri, params.textDocument.languageId, params.textDocument.version, params.textDocument.text)));
    // Mark the file as dirty as we don't know what changes were made to it
    frontend.markDirty(moduleName);
}

void WorkspaceFolder::updateTextDocument(
    const lsp::DocumentUri& uri, const lsp::DidChangeTextDocumentParams& params, std::vector<Luau::ModuleName>* markedDirty)
{
    auto moduleName = fileResolver.getModuleName(uri);

    if (!contains(fileResolver.managedFiles, moduleName))
    {
        client->sendLogMessage(lsp::MessageType::Error, "Text Document not loaded locally: " + uri.toString());
        return;
    }
    auto& textDocument = fileResolver.managedFiles.at(moduleName);
    textDocument.update(params.contentChanges, params.textDocument.version);

    // Mark the module dirty for the typechecker
    frontend.markDirty(moduleName, markedDirty);
}

void WorkspaceFolder::closeTextDocument(const lsp::DocumentUri& uri)
{
    auto config = client->getConfiguration(rootUri);
    auto moduleName = fileResolver.getModuleName(uri);
    fileResolver.managedFiles.erase(moduleName);

    // Clear out base uri fsPath as well, in case we managed it like that
    // TODO: can be potentially removed when server generates sourcemap
    fileResolver.managedFiles.erase(uri.fsPath().generic_string());

    // Mark the module as dirty as we no longer track its changes
    frontend.markDirty(moduleName);

    // Refresh workspace diagnostics to clear diagnostics on ignored files
    if (!config.diagnostics.workspace || isIgnoredFile(uri.fsPath()))
    {
        if (client->workspaceDiagnosticsToken)
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
        instanceTypes.clear();
        types::registerInstanceTypes(frontend.typeChecker, instanceTypes, fileResolver, /* TODO - expressiveTypes: */ false);
        types::registerInstanceTypes(frontend.typeCheckerForAutocomplete, instanceTypes, fileResolver, /* TODO - expressiveTypes: */ true);

        // Update managed file paths as they may be converted to virtual
        // Check if we have the original file URIs stored (https://github.com/JohnnyMorganz/luau-lsp/issues/26)
        for (auto& [filePath, textDocument] : fileResolver.managedFiles)
        {
            if (!fileResolver.isVirtualPath(filePath))
            {
                if (auto virtualPath = fileResolver.resolveToVirtualPath(filePath); virtualPath && virtualPath != filePath)
                {
                    // Change the managed file key to use the new modulename
                    auto nh = fileResolver.managedFiles.extract(filePath);
                    nh.key() = *virtualPath;
                    fileResolver.managedFiles.insert(std::move(nh));
                }
            }
        }

        // Signal diagnostics refresh
        client->terminateWorkspaceDiagnostics();
        client->refreshWorkspaceDiagnostics();

        return true;
    }
    else
    {
        return false;
    }
}

void WorkspaceFolder::initialize()
{
    Luau::registerBuiltinTypes(frontend.typeChecker);
    Luau::registerBuiltinTypes(frontend.typeCheckerForAutocomplete);

    if (client->definitionsFiles.empty())
    {
        client->sendLogMessage(lsp::MessageType::Warning, "No definitions file provided by client");
    }

    for (auto definitionsFile : client->definitionsFiles)
    {
        client->sendLogMessage(lsp::MessageType::Info, "Loading definitions file: " + definitionsFile.generic_string());
        auto result = types::registerDefinitions(frontend.typeChecker, definitionsFile);
        types::registerDefinitions(frontend.typeCheckerForAutocomplete, definitionsFile);

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
    Luau::freeze(frontend.typeChecker.globalTypes);
    Luau::freeze(frontend.typeCheckerForAutocomplete.globalTypes);
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
}