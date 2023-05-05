#include "LSP/LanguageServer.hpp"

#include <string>
#include <variant>
#include <exception>
#include <algorithm>

#include "LSP/Uri.hpp"
#include "LSP/DocumentationParser.hpp"

#define ASSERT_PARAMS(params, method) \
    if (!params) \
        throw json_rpc::JsonRpcException(lsp::ErrorCode::InvalidParams, "params not provided for " method);

#define REQUIRED_PARAMS(params, method) \
    (!(params) ? throw json_rpc::JsonRpcException(lsp::ErrorCode::InvalidParams, "params not provided for " method) : (params).value())

LanguageServer::LanguageServer(const std::vector<std::filesystem::path>& definitionsFiles,
    const std::vector<std::filesystem::path>& documentationFiles, const std::optional<Luau::Config>& defaultConfig)
    : client(std::make_shared<Client>())
    , defaultConfig(defaultConfig)
{
    client->definitionsFiles = definitionsFiles;
    client->documentationFiles = documentationFiles;
    parseDocumentation(documentationFiles, client->documentation, client);
    nullWorkspace = std::make_shared<WorkspaceFolder>(client, "$NULL_WORKSPACE", Uri(), defaultConfig);
}

/// Finds the workspace which the file belongs to.
/// If no workspace is found, the file is attached to the null workspace
WorkspaceFolderPtr LanguageServer::findWorkspace(const lsp::DocumentUri& file)
{
    if (file == nullWorkspace->rootUri)
        return nullWorkspace;

    WorkspaceFolderPtr bestWorkspace = nullptr;
    size_t length = 0;
    auto checkStr = file.toString();

    for (auto& workspace : workspaceFolders)
    {
        if (file == workspace->rootUri)
            return workspace;

        // Check if the root uri is a prefix of the file
        auto prefixStr = workspace->rootUri.toString();
        auto size = prefixStr.size();
        if (size < length)
            continue;

        if (checkStr.compare(0, size, prefixStr) == 0)
        {
            bestWorkspace = workspace;
            length = size;
        }
    }

    if (bestWorkspace)
        return bestWorkspace;

    client->sendTrace("cannot find workspace for " + file.toString());
    return nullWorkspace;
}

lsp::ServerCapabilities LanguageServer::getServerCapabilities()
{
    lsp::ServerCapabilities capabilities;
    capabilities.textDocumentSync = lsp::TextDocumentSyncKind::Incremental;
    // Completion
    std::vector<std::string> completionTriggerCharacters{".", ":", "'", "\"", "/", "\n"}; // \n is used to trigger end completion
    lsp::CompletionOptions::CompletionItem completionItem{/* labelDetailsSupport: */ true};
    capabilities.completionProvider = {completionTriggerCharacters, std::nullopt, /* resolveProvider: */ false, completionItem};
    // Hover Provider
    capabilities.hoverProvider = true;
    // Signature Help
    std::vector<std::string> signatureHelpTriggerCharacters{"(", ","};
    capabilities.signatureHelpProvider = {signatureHelpTriggerCharacters};
    // Go To Declaration Provider
    capabilities.declarationProvider = false; // TODO: does this apply to Luau?
    // Go To Definition Provider
    capabilities.definitionProvider = true;
    // Go To Type Definition Provider
    capabilities.typeDefinitionProvider = true;
    // Go To Implementation Provider
    capabilities.implementationProvider = false; // TODO: does this apply to Luau?
    // Find References Provider
    capabilities.referencesProvider = true;
    // Document Symbol Provider
    capabilities.documentSymbolProvider = true;
    // Color Provider
    capabilities.colorProvider = true;
    // Document Link Provider
    capabilities.documentLinkProvider = {false};
    // Code Action Provider
    capabilities.codeActionProvider = {std::vector<lsp::CodeActionKind>{lsp::CodeActionKind::SourceOrganizeImports}, /* resolveProvider: */ false};
    // Rename Provider
    capabilities.renameProvider = true;
    // Inlay Hint Provider
    capabilities.inlayHintProvider = true;
    // Diagnostics Provider
    capabilities.diagnosticProvider = {"luau", /* interFileDependencies: */ true, /* workspaceDiagnostics: */ true};
    // Workspace Symbols Provider
    capabilities.workspaceSymbolProvider = true;
    // Semantic Tokens Provider
    capabilities.semanticTokensProvider = {
        {
            std::vector<lsp::SemanticTokenTypes>(std::begin(lsp::SemanticTokenTypesList), std::end(lsp::SemanticTokenTypesList)),
            std::vector<lsp::SemanticTokenModifiers>(std::begin(lsp::SemanticTokenModifiersList), std::end(lsp::SemanticTokenModifiersList)),
        },
        /* range: */ false,
        /* full: */ true,
    };
    // Workspaces
    lsp::WorkspaceFoldersServerCapabilities workspaceFolderCapabilities{true, false};
    capabilities.workspace = lsp::WorkspaceCapabilities{workspaceFolderCapabilities};
    return capabilities;
}

void LanguageServer::onRequest(const id_type& id, const std::string& method, std::optional<json> baseParams)
{
    // Handle request
    // If a request has been sent before the server is initialized, we should error
    if (!isInitialized && method != "initialize")
        throw JsonRpcException(lsp::ErrorCode::ServerNotInitialized, "server not initialized");
    // If we received a request after a shutdown, then we error with InvalidRequest
    if (shutdownRequested)
        throw JsonRpcException(lsp::ErrorCode::InvalidRequest, "server is shutting down");

    Response response;

    if (method == "initialize")
    {
        response = onInitialize(REQUIRED_PARAMS(baseParams, "initialize"));
    }
    else if (method == "shutdown")
    {
        response = onShutdown(id);
    }
    else if (method == "textDocument/completion")
    {
        response = completion(REQUIRED_PARAMS(baseParams, "textDocument/completion"));
    }
    else if (method == "textDocument/documentLink")
    {
        response = documentLink(REQUIRED_PARAMS(baseParams, "textDocument/documentLink"));
    }
    else if (method == "textDocument/hover")
    {
        response = hover(REQUIRED_PARAMS(baseParams, "textDocument/hover"));
    }
    else if (method == "textDocument/signatureHelp")
    {
        response = signatureHelp(REQUIRED_PARAMS(baseParams, "textDocument/signatureHelp"));
    }
    else if (method == "textDocument/definition")
    {
        response = gotoDefinition(REQUIRED_PARAMS(baseParams, "textDocument/definition"));
    }
    else if (method == "textDocument/typeDefinition")
    {
        response = gotoTypeDefinition(REQUIRED_PARAMS(baseParams, "textDocument/typeDefinition"));
    }
    else if (method == "textDocument/references")
    {
        response = references(REQUIRED_PARAMS(baseParams, "textDocument/references"));
    }
    else if (method == "textDocument/rename")
    {
        response = rename(REQUIRED_PARAMS(baseParams, "textDocument/rename"));
    }
    else if (method == "textDocument/documentSymbol")
    {
        response = documentSymbol(REQUIRED_PARAMS(baseParams, "textDocument/documentSymbol"));
    }
    else if (method == "textDocument/codeAction")
    {
        response = codeAction(REQUIRED_PARAMS(baseParams, "textDocument/codeAction"));
    }
    // else if (method == "codeAction/resolve")
    // {
    //     response = codeActionResolve(REQUIRED_PARAMS(params, "codeAction/resolve"));
    // }
    else if (method == "textDocument/semanticTokens/full")
    {
        response = semanticTokens(REQUIRED_PARAMS(baseParams, "textDocument/semanticTokns/full"));
    }
    else if (method == "textDocument/inlayHint")
    {
        response = inlayHint(REQUIRED_PARAMS(baseParams, "textDocument/inlayHint"));
    }
    else if (method == "textDocument/documentColor")
    {
        response = documentColor(REQUIRED_PARAMS(baseParams, "textDocument/documentColor"));
    }
    else if (method == "textDocument/colorPresentation")
    {
        response = colorPresentation(REQUIRED_PARAMS(baseParams, "textDocument/colorPresentation"));
    }
    else if (method == "textDocument/diagnostic")
    {
        response = documentDiagnostic(REQUIRED_PARAMS(baseParams, "textDocument/diagnostic"));
    }
    else if (method == "workspace/diagnostic")
    {
        // This request has partial request support.
        // If workspaceDiagnostic returns nothing, then we don't signal a response (as data will be sent as progress notifications)
        if (auto report = workspaceDiagnostic(REQUIRED_PARAMS(baseParams, "workspace/diagnostic")))
        {
            response = report;
        }
        else
        {
            client->workspaceDiagnosticsRequestId = id;
            return;
        }
    }
    else if (method == "workspace/symbol")
    {
        ASSERT_PARAMS(baseParams, "workspace/symbol");
        auto params = baseParams->get<lsp::WorkspaceSymbolParams>();

        std::vector<lsp::WorkspaceSymbol> result;
        for (auto& workspace : workspaceFolders)
        {
            auto report = workspace->workspaceSymbol(params);
            if (report)
                result.insert(result.end(), std::make_move_iterator(report->begin()), std::make_move_iterator(report->end()));
        }
        response = result;
    }
    else
    {
        throw JsonRpcException(lsp::ErrorCode::MethodNotFound, "method not found / supported: " + method);
    }

    client->sendResponse(id, response);
}

void LanguageServer::onNotification(const std::string& method, std::optional<json> params)
{
    // Handle notification
    // If a notification is sent before the server is initilized or after a shutdown is requested (unless its exit), we should
    // drop it
    if ((!isInitialized || shutdownRequested) && method != "exit")
        return;

    if (method == "exit")
    {
        // Exit the process loop
        std::exit(shutdownRequested ? 0 : 1);
    }
    else if (method == "initialized")
    {
        onInitialized(REQUIRED_PARAMS(params, "initialized"));
    }
    else if (method == "$/setTrace")
    {
        client->setTrace(REQUIRED_PARAMS(params, "$/setTrace"));
    }
    else if (method == "$/cancelRequest")
    {
        // NO-OP
        // TODO: support cancellation
    }
    else if (method == "textDocument/didOpen")
    {
        onDidOpenTextDocument(REQUIRED_PARAMS(params, "textDocument/didOpen"));
    }
    else if (method == "textDocument/didChange")
    {
        onDidChangeTextDocument(REQUIRED_PARAMS(params, "textDocument/didChange"));
    }
    else if (method == "textDocument/didSave")
    {
        // NO-OP
    }
    else if (method == "textDocument/didClose")
    {
        onDidCloseTextDocument(REQUIRED_PARAMS(params, "textDocument/didClose"));
    }
    else if (method == "workspace/didChangeConfiguration")
    {
        onDidChangeConfiguration(REQUIRED_PARAMS(params, "workspace/didChangeConfiguration"));
    }
    else if (method == "workspace/didChangeWorkspaceFolders")
    {
        onDidChangeWorkspaceFolders(REQUIRED_PARAMS(params, "workspace/didChangeWorkspaceFolders"));
    }
    else if (method == "workspace/didChangeWatchedFiles")
    {
        onDidChangeWatchedFiles(REQUIRED_PARAMS(params, "workspace/didChangeWatchedFiles"));
    }
    else if (method == "$/plugin/full")
    {
        onStudioPluginFullChange(REQUIRED_PARAMS(params, "$/plugin/full"));
    }
    else if (method == "$/plugin/clear")
    {
        onStudioPluginClear();
    }
    else
    {
        client->sendLogMessage(lsp::MessageType::Warning, "unknown notification method: " + method);
    }
}

void LanguageServer::processInputLoop()
{
    std::string jsonString;
    while (std::cin)
    {
        if (client->readRawMessage(jsonString))
        {
            // sendTrace(jsonString, std::nullopt);
            std::optional<id_type> id = std::nullopt;
            try
            {
                // Parse the input
                auto msg = json_rpc::parse(jsonString);
                id = msg.id;

                if (msg.is_request())
                {
                    onRequest(msg.id.value(), msg.method.value(), msg.params);
                }
                else if (msg.is_response())
                {
                    client->handleResponse(msg);
                }
                else if (msg.is_notification())
                {
                    onNotification(msg.method.value(), msg.params);
                }
                else
                {
                    throw JsonRpcException(lsp::ErrorCode::InvalidRequest, "invalid json-rpc message");
                }
            }
            catch (const JsonRpcException& e)
            {
                client->sendError(id, e);
            }
            catch (const json::exception& e)
            {
                client->sendError(id, JsonRpcException(lsp::ErrorCode::ParseError, e.what()));
            }
            catch (const std::exception& e)
            {
                client->sendError(id, JsonRpcException(lsp::ErrorCode::InternalError, e.what()));
            }
        }
    }
}

bool LanguageServer::requestedShutdown()
{
    return shutdownRequested;
}

// Dispatch handlers
lsp::InitializeResult LanguageServer::onInitialize(const lsp::InitializeParams& params)
{
    // Set provided settings
    client->sendTrace("client capabilities: " + json(params.capabilities).dump(), std::nullopt);
    client->capabilities = params.capabilities;
    client->traceMode = params.trace;

    // Configure workspaces
    if (params.workspaceFolders.has_value())
    {
        for (auto& folder : params.workspaceFolders.value())
        {
            workspaceFolders.push_back(std::make_shared<WorkspaceFolder>(client, folder.name, folder.uri, defaultConfig));
        }
    }
    else if (params.rootUri.has_value())
    {
        workspaceFolders.push_back(std::make_shared<WorkspaceFolder>(client, "$ROOT", params.rootUri.value(), defaultConfig));
    }

    isInitialized = true;
    lsp::InitializeResult result;
    result.capabilities = getServerCapabilities();

    // Position Encoding
    if (client->capabilities.general && client->capabilities.general->positionEncodings &&
        client->capabilities.general->positionEncodings->size() > 0)
    {
        auto& encodings = *client->capabilities.general->positionEncodings;
        if (encodings[0] == lsp::PositionEncodingKind::UTF8 || encodings[0] == lsp::PositionEncodingKind::UTF16)
            positionEncoding() = encodings[0];
    }
    result.capabilities.positionEncoding = positionEncoding();
    client->sendLogMessage(lsp::MessageType::Info, "negotiated position encoding: " + json(positionEncoding()).dump());

    client->sendTrace("server capabilities:" + json(result).dump(), std::nullopt);
    return result;
}

void LanguageServer::onInitialized(const lsp::InitializedParams& params)
{
    // Client received result of initialize
    client->sendLogMessage(lsp::MessageType::Info, "server initialized!");
    client->sendLogMessage(lsp::MessageType::Info, "trace level: " + json(client->traceMode).dump());

    // Initialise loaded workspaces
    nullWorkspace->initialize();
    for (auto& folder : workspaceFolders)
    {
        folder->initialize();
    }

    // Dynamically register for configuration changed notifications
    if (client->capabilities.workspace && client->capabilities.workspace->didChangeConfiguration &&
        client->capabilities.workspace->didChangeConfiguration->dynamicRegistration)
    {
        client->registerCapability("didChangeConfigurationCapability", "workspace/didChangeConfiguration", nullptr);
        client->configChangedCallback =
            [&](const lsp::DocumentUri& workspaceUri, const ClientConfiguration& config, const ClientConfiguration* oldConfig)
        {
            auto workspace = findWorkspace(workspaceUri);

            // Update the workspace setup with the new configuration
            workspace->setupWithConfiguration(config);

            // Refresh diagnostics
            this->recomputeDiagnostics(workspace, config);

            // Refresh inlay hint if changed
            if (!oldConfig || oldConfig->inlayHints != config.inlayHints)
                client->refreshInlayHints();
        };

        // Send off requests to get the configuration for each workspace
        std::vector<lsp::DocumentUri> items{nullWorkspace->rootUri};
        for (auto& workspace : workspaceFolders)
            items.emplace_back(workspace->rootUri);
        client->requestConfiguration(items);
    }
    else
    {
        // Client does not support retrieving configuration information, so we just setup the workspaces with the default, global, configuration
        for (auto& folder : workspaceFolders)
        {
            folder->setupWithConfiguration(client->globalConfig);
        }
    }

    // Dynamically register file watchers
    if (client->capabilities.workspace && client->capabilities.workspace->didChangeWatchedFiles &&
        client->capabilities.workspace->didChangeWatchedFiles->dynamicRegistration)
    {
        client->sendLogMessage(lsp::MessageType::Info, "registering didChangedWatchedFiles capability");

        std::vector<lsp::FileSystemWatcher> watchers{};
        watchers.push_back(lsp::FileSystemWatcher{"**/.luaurc"});
        watchers.push_back(lsp::FileSystemWatcher{"**/sourcemap.json"});
        watchers.push_back(lsp::FileSystemWatcher{"**/*.{lua,luau}"});
        client->registerCapability(
            "didChangedWatchedFilesCapability", "workspace/didChangeWatchedFiles", lsp::DidChangeWatchedFilesRegistrationOptions{watchers});
    }
    else
    {
        client->sendLogMessage(lsp::MessageType::Warning,
            "client does not allow didChangeWatchedFiles registration - automatic updating on sourcemap/config changes will not be provided");
    }
}

void LanguageServer::pushDiagnostics(WorkspaceFolderPtr& workspace, const lsp::DocumentUri& uri, const size_t version)
{
    // Convert the diagnostics report into a series of diagnostics published for each relevant file
    lsp::DocumentDiagnosticParams params{lsp::TextDocumentIdentifier{uri}};
    auto diagnostics = workspace->documentDiagnostics(params);
    client->publishDiagnostics(lsp::PublishDiagnosticsParams{uri, version, diagnostics.items});

    if (!diagnostics.relatedDocuments.empty())
    {
        for (const auto& [uri, diagnostics] : diagnostics.relatedDocuments)
        {
            if (diagnostics.kind == lsp::DocumentDiagnosticReportKind::Full)
            {
                client->publishDiagnostics(lsp::PublishDiagnosticsParams{Uri::parse(uri), std::nullopt, diagnostics.items});
            }
        }
    }
}

/// Recompute all necessary diagnostics when we detect a configuration (or sourcemap) change
void LanguageServer::recomputeDiagnostics(WorkspaceFolderPtr& workspace, const ClientConfiguration& config)
{
    // Handle diagnostics if in push-mode
    if ((!client->capabilities.textDocument || !client->capabilities.textDocument->diagnostic))
    {
        // Recompute workspace diagnostics if requested
        if (config.diagnostics.workspace)
        {
            auto diagnostics = workspace->workspaceDiagnostics({});
            for (const auto& report : diagnostics.items)
            {
                if (report.kind == lsp::DocumentDiagnosticReportKind::Full)
                {
                    client->publishDiagnostics(lsp::PublishDiagnosticsParams{report.uri, report.version, report.items});
                }
            }
        }
        // Recompute diagnostics for all currently opened files
        else
        {
            for (const auto& [_, document] : workspace->fileResolver.managedFiles)
                this->pushDiagnostics(workspace, document.uri(), document.version());
        }
    }
    else
    {
        client->terminateWorkspaceDiagnostics();
        client->refreshWorkspaceDiagnostics();
    }
}

void LanguageServer::onDidOpenTextDocument(const lsp::DidOpenTextDocumentParams& params)
{
    // Start managing the file in-memory
    auto workspace = findWorkspace(params.textDocument.uri);
    workspace->openTextDocument(params.textDocument.uri, params);

    // Trigger diagnostics
    // By default, we rely on the pull based diagnostics model (based on documentDiagnostic)
    // however if a client doesn't yet support it, we push the diagnostics instead
    if (!client->capabilities.textDocument || !client->capabilities.textDocument->diagnostic)
    {
        pushDiagnostics(workspace, params.textDocument.uri, params.textDocument.version);
    }
}

void LanguageServer::onDidChangeTextDocument(const lsp::DidChangeTextDocumentParams& params)
{
    // Keep a vector of reverse dependencies marked dirty to extend diagnostics for them
    std::vector<Luau::ModuleName> markedDirty{};

    // Update in-memory file with new contents
    auto workspace = findWorkspace(params.textDocument.uri);
    workspace->updateTextDocument(params.textDocument.uri, params, &markedDirty);

    // Trigger diagnostics
    // By default, we rely on the pull based diagnostics model (based on documentDiagnostic)
    // however if a client doesn't yet support it, we push the diagnostics instead
    if (!client->capabilities.textDocument || !client->capabilities.textDocument->diagnostic)
    {
        // Convert the diagnostics report into a series of diagnostics published for each relevant file
        auto diagnostics = workspace->documentDiagnostics(lsp::DocumentDiagnosticParams{{params.textDocument.uri}});
        client->publishDiagnostics(lsp::PublishDiagnosticsParams{params.textDocument.uri, params.textDocument.version, diagnostics.items});

        // Compute diagnostics for reverse dependencies
        // TODO: should we put this inside documentDiagnostics so it works in the pull based model as well? (its a reverse BFS which is expensive)
        // TODO: maybe this should only be done onSave
        auto config = client->getConfiguration(workspace->rootUri);
        if (config.diagnostics.includeDependents || config.diagnostics.workspace)
        {
            std::unordered_map<std::string, lsp::SingleDocumentDiagnosticReport> reverseDependencyDiagnostics{};
            for (auto& module : markedDirty)
            {
                auto filePath = workspace->fileResolver.resolveToRealPath(module);
                if (filePath)
                {
                    auto uri = Uri::file(*filePath);
                    if (uri != params.textDocument.uri && !contains(diagnostics.relatedDocuments, uri.toString()) &&
                        !workspace->isIgnoredFile(*filePath, config))
                    {
                        auto dependencyDiags = workspace->documentDiagnostics(lsp::DocumentDiagnosticParams{{uri}});
                        diagnostics.relatedDocuments.emplace(uri.toString(),
                            lsp::SingleDocumentDiagnosticReport{dependencyDiags.kind, dependencyDiags.resultId, dependencyDiags.items});
                        diagnostics.relatedDocuments.merge(dependencyDiags.relatedDocuments);
                    }
                }
            }
        }

        if (!diagnostics.relatedDocuments.empty())
        {
            for (const auto& [uri, diagnostics] : diagnostics.relatedDocuments)
            {
                if (diagnostics.kind == lsp::DocumentDiagnosticReportKind::Full)
                {
                    client->publishDiagnostics(lsp::PublishDiagnosticsParams{Uri::parse(uri), std::nullopt, diagnostics.items});
                }
            }
        }
    }
}

void LanguageServer::onDidCloseTextDocument(const lsp::DidCloseTextDocumentParams& params)
{
    // Release managed in-memory file
    auto workspace = findWorkspace(params.textDocument.uri);
    workspace->closeTextDocument(params.textDocument.uri);
}

void LanguageServer::onDidChangeConfiguration(const lsp::DidChangeConfigurationParams& params)
{
    // We can't tell what workspace this is for, so we will have to clear our config information and
    // manually get it for all the workspaces again
    if (client->capabilities.workspace && client->capabilities.workspace->configuration)
    {
        client->configStore.clear();

        // Send off requests to get the configuration again for each workspace
        std::vector<lsp::DocumentUri> items{nullWorkspace->rootUri};
        for (auto& workspace : workspaceFolders)
            items.emplace_back(workspace->rootUri);
        client->requestConfiguration(items);
    }
    else
    {
        // We just have to assume these are the new global settings
        // We can't assume its formed correctly, so lets wrap it in a try-catch
        try
        {
            client->globalConfig = params.settings;
        }
        catch (const std::exception& e)
        {
            client->sendLogMessage(lsp::MessageType::Error, std::string("failed to refresh global configuration: ") + e.what());
        }
    }
}

void LanguageServer::onDidChangeWorkspaceFolders(const lsp::DidChangeWorkspaceFoldersParams& params)
{
    // Erase all old folders (this is O(n^2))
    for (auto it = workspaceFolders.begin(); it != workspaceFolders.end();)
    {
        auto name = (*it)->name;
        if (std::find_if(params.event.removed.begin(), params.event.removed.end(),
                [name](auto& w)
                {
                    return w.name == name;
                }) != std::end(params.event.removed))
        {
            it = workspaceFolders.erase(it);

            // Remove the configuration information for this folder
            client->removeConfiguration((*it)->rootUri);
        }
        else
        {
            ++it;
        }
    }

    // Add new folders
    std::vector<lsp::DocumentUri> configItems{};
    for (auto& folder : params.event.added)
    {
        workspaceFolders.emplace_back(std::make_shared<WorkspaceFolder>(client, folder.name, folder.uri, defaultConfig));
        configItems.emplace_back(folder.uri);
    }
    client->requestConfiguration(configItems);
}

void LanguageServer::onDidChangeWatchedFiles(const lsp::DidChangeWatchedFilesParams& params)
{
    for (const auto& change : params.changes)
    {
        auto workspace = findWorkspace(change.uri);
        auto config = client->getConfiguration(workspace->rootUri);
        auto filePath = change.uri.fsPath();

        // Flag sourcemap changes
        if (filePath.filename() == "sourcemap.json")
        {
            client->sendLogMessage(lsp::MessageType::Info, "Registering sourcemap changed for workspace " + workspace->name);
            workspace->updateSourceMap();

            // Recompute diagnostics
            this->recomputeDiagnostics(workspace, config);
        }
        else if (filePath.filename() == ".luaurc")
        {
            client->sendLogMessage(
                lsp::MessageType::Info, "Acknowledge config changed for workspace " + workspace->name + ", clearing configuration cache");
            workspace->fileResolver.clearConfigCache();

            // Recompute diagnostics
            this->recomputeDiagnostics(workspace, config);
        }
        else if (filePath.extension() == ".lua" || filePath.extension() == ".luau")
        {
            // Index the workspace on changes
            if (config.index.enabled && workspace->isConfigured)
            {
                auto moduleName = workspace->fileResolver.getModuleName(change.uri);

                std::vector<Luau::ModuleName> markedDirty{};
                workspace->frontend.markDirty(moduleName, &markedDirty);

                if (change.type == lsp::FileChangeType::Created)
                    workspace->frontend.check(moduleName);

                // Re-check the reverse dependencies
                for (const auto& moduleName : markedDirty)
                    workspace->frontend.check(moduleName);
            }

            // Clear the diagnostics for the file in case it was not managed
            if (change.type == lsp::FileChangeType::Deleted)
                workspace->clearDiagnosticsForFile(change.uri);
        }
    }
}

Response LanguageServer::onShutdown(const id_type& id)
{
    shutdownRequested = true;
    return nullptr;
}
