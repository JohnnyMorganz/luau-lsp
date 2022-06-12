#include <iostream>
#include <fstream>
#include <string>
#include <optional>
#include <variant>
#include <exception>
#include <filesystem>
#include <algorithm>
#include "LSP/LanguageServer.hpp"

using json = nlohmann::json;
using namespace json_rpc;
using Response = json;
using WorkspaceFolderPtr = std::shared_ptr<WorkspaceFolder>;
using ClientPtr = std::shared_ptr<Client>;

#define REQUIRED_PARAMS(params, method) \
    !params ? throw json_rpc::JsonRpcException(lsp::ErrorCode::InvalidParams, "params not provided for " method) : params.value()

bool replace(std::string& str, const std::string& from, const std::string& to)
{
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

/// Finds the workspace which the file belongs to.
/// If no workspace is found, the file is attached to the null workspace
WorkspaceFolderPtr LanguageServer::findWorkspace(const lsp::DocumentUri file)
{
    for (auto& workspace : workspaceFolders)
    {
        if (workspace->isInWorkspace(file))
        {
            return workspace; // TODO: should we return early here? maybe a better match comes along?
        }
    }
    client->sendLogMessage(lsp::MessageType::Info, "cannot find workspace for " + file.toString());
    return nullWorkspace;
}

lsp::ServerCapabilities LanguageServer::getServerCapabilities()
{
    lsp::ServerCapabilities capabilities;
    capabilities.textDocumentSync = lsp::TextDocumentSyncKind::Incremental;
    // Completion
    std::vector<std::string> completionTriggerCharacters{".", ":", "'", "\"", "\n"}; // \n is used to trigger end completion
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
    // Document Link Provider
    capabilities.documentLinkProvider = {false};
    // Rename Provider
    capabilities.renameProvider = true;
    // Diagnostics Provider
    capabilities.diagnosticProvider = {"luau", /* interFileDependencies: */ true, /* workspaceDiagnostics: */ true};
    // Workspaces
    lsp::WorkspaceFoldersServerCapabilities workspaceFolderCapabilities{true, false};
    capabilities.workspace = lsp::WorkspaceCapabilities{workspaceFolderCapabilities};
    return capabilities;
}

Response LanguageServer::onRequest(const id_type& id, const std::string& method, std::optional<json> params)
{
    // Handle request
    // If a request has been sent before the server is initialized, we should error
    if (!isInitialized && method != "initialize")
        throw JsonRpcException(lsp::ErrorCode::ServerNotInitialized, "server not initialized");
    // If we received a request after a shutdown, then we error with InvalidRequest
    if (shutdownRequested)
        throw JsonRpcException(lsp::ErrorCode::InvalidRequest, "server is shutting down");

    if (method == "initialize")
    {
        return onInitialize(REQUIRED_PARAMS(params, "initialize"));
    }
    else if (method == "shutdown")
    {
        return onShutdown(id);
    }
    else if (method == "textDocument/completion")
    {
        return completion(REQUIRED_PARAMS(params, "textDocument/completion"));
    }
    else if (method == "textDocument/documentLink")
    {
        return documentLink(REQUIRED_PARAMS(params, "textDocument/documentLink"));
    }
    else if (method == "textDocument/hover")
    {
        return hover(REQUIRED_PARAMS(params, "textDocument/hover"));
    }
    else if (method == "textDocument/signatureHelp")
    {
        return signatureHelp(REQUIRED_PARAMS(params, "textDocument/signatureHelp"));
    }
    else if (method == "textDocument/definition")
    {
        return gotoDefinition(REQUIRED_PARAMS(params, "textDocument/definition"));
    }
    else if (method == "textDocument/typeDefinition")
    {
        return gotoTypeDefinition(REQUIRED_PARAMS(params, "textDocument/typeDefinition"));
    }
    else if (method == "textDocument/references")
    {
        return references(REQUIRED_PARAMS(params, "textDocument/references"));
    }
    else if (method == "textDocument/rename")
    {
        return rename(REQUIRED_PARAMS(params, "textDocument/rename"));
    }
    else if (method == "textDocument/documentSymbol")
    {
        return documentSymbol(REQUIRED_PARAMS(params, "textDocument/documentSymbol"));
    }
    else if (method == "textDocument/diagnostic")
    {
        return documentDiagnostic(REQUIRED_PARAMS(params, "textDocument/diagnostic"));
    }
    else if (method == "workspace/diagnostic")
    {
        return workspaceDiagnostic(REQUIRED_PARAMS(params, "workspace/diagnostic"));
    }
    else
    {
        throw JsonRpcException(lsp::ErrorCode::MethodNotFound, "method not found / supported: " + method);
    }
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
    else if (method == "textDocument/didOpen")
    {
        onDidOpenTextDocument(REQUIRED_PARAMS(params, "textDocument/didOpen"));
    }
    else if (method == "textDocument/didChange")
    {
        onDidChangeTextDocument(REQUIRED_PARAMS(params, "textDocument/didChange"));
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
                    auto response = onRequest(msg.id.value(), msg.method.value(), msg.params);
                    // sendTrace(response.dump(), std::nullopt);
                    client->sendResponse(msg.id.value(), response);
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
            workspaceFolders.push_back(std::make_shared<WorkspaceFolder>(client, folder.name, folder.uri));
        }
    }
    else if (params.rootUri.has_value())
    {
        workspaceFolders.push_back(std::make_shared<WorkspaceFolder>(client, "$ROOT", params.rootUri.value()));
    }

    isInitialized = true;
    lsp::InitializeResult result;
    result.capabilities = getServerCapabilities();
    client->sendTrace("server capabilities:" + json(result).dump(), std::nullopt);
    return result;
}

void LanguageServer::onInitialized(const lsp::InitializedParams& params)
{
    // Client received result of initialize
    client->sendLogMessage(lsp::MessageType::Info, "server initialized!");
    client->sendLogMessage(lsp::MessageType::Info, "trace level: " + json(client->traceMode).dump());

    // Dynamically register for configuration changed notifications
    if (client->capabilities.workspace && client->capabilities.workspace->didChangeConfiguration &&
        client->capabilities.workspace->didChangeConfiguration->dynamicRegistration)
    {
        client->registerCapability("didChangeConfigurationCapability", "workspace/didChangeConfiguration", nullptr);
        client->configChangedCallback = [&](const lsp::DocumentUri& workspaceUri, const ClientConfiguration& config)
        {
            auto workspace = findWorkspace(workspaceUri);

            // Recompute workspace diagnostics if requested
            if (config.diagnostics.workspace)
            {
                auto diagnostics = workspace->workspaceDiagnostics({});
                for (const auto& report : diagnostics.items)
                {
                    if (report.kind == lsp::DocumentDiagnosticReportKind::Full)
                    {
                        client->sendNotification(
                            "textDocument/publishDiagnostics", lsp::PublishDiagnosticsParams{report.uri, report.version, report.items});
                    }
                }
            }
        };

        // Send off requests to get the configuration for each workspace
        std::vector<lsp::DocumentUri> items;
        for (auto& workspace : workspaceFolders)
        {
            items.emplace_back(workspace->rootUri);
        }
        client->requestConfiguration(items);
    }

    // Dynamically register file watchers
    if (client->capabilities.workspace && client->capabilities.workspace->didChangeWatchedFiles &&
        client->capabilities.workspace->didChangeWatchedFiles->dynamicRegistration)
    {
        client->sendLogMessage(lsp::MessageType::Info, "registering didChangedWatchedFiles capability");

        std::vector<lsp::FileSystemWatcher> watchers;
        watchers.push_back(lsp::FileSystemWatcher{"**/.luaurc"});
        watchers.push_back(lsp::FileSystemWatcher{"**/sourcemap.json"});
        client->registerCapability(
            "didChangedWatchedFilesCapability", "workspace/didChangeWatchedFiles", lsp::DidChangeWatchedFilesRegistrationOptions{watchers});
    }
    else
    {
        client->sendLogMessage(lsp::MessageType::Warning,
            "client does not allow didChangeWatchedFiles registration - automatic updating on sourcemap/config changes will not be provided");
    }
}

void LanguageServer::pushDiagnostics(WorkspaceFolderPtr& workspace, const lsp::DocumentUri& uri, const int version)
{
    // Convert the diagnostics report into a series of diagnostics published for each relevant file
    lsp::DocumentDiagnosticParams params{lsp::TextDocumentIdentifier{uri}};
    auto diagnostics = workspace->documentDiagnostics(params);
    client->sendNotification("textDocument/publishDiagnostics", lsp::PublishDiagnosticsParams{uri, version, diagnostics.items});

    if (!diagnostics.relatedDocuments.empty())
    {
        for (const auto& [uri, diagnostics] : diagnostics.relatedDocuments)
        {
            if (diagnostics.kind == lsp::DocumentDiagnosticReportKind::Full)
            {
                client->sendNotification(
                    "textDocument/publishDiagnostics", lsp::PublishDiagnosticsParams{Uri::parse(uri), std::nullopt, diagnostics.items});
            }
        }
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
    std::vector<Luau::ModuleName> markedDirty;

    // Update in-memory file with new contents
    auto workspace = findWorkspace(params.textDocument.uri);
    workspace->updateTextDocument(params.textDocument.uri, params, &markedDirty);

    // Trigger diagnostics
    // By default, we rely on the pull based diagnostics model (based on documentDiagnostic)
    // however if a client doesn't yet support it, we push the diagnostics instead
    if (!client->capabilities.textDocument || !client->capabilities.textDocument->diagnostic)
    {
        // Convert the diagnostics report into a series of diagnostics published for each relevant file
        auto diagnostics = workspace->documentDiagnostics(lsp::DocumentDiagnosticParams{params.textDocument});
        client->sendNotification("textDocument/publishDiagnostics",
            lsp::PublishDiagnosticsParams{params.textDocument.uri, params.textDocument.version, diagnostics.items});

        // Compute diagnostics for reverse dependencies
        // TODO: should we put this inside documentDiagnostics so it works in the pull based model as well? (its a reverse BFS which is expensive)
        // TODO: maybe this should only be done onSave
        auto config = client->getConfiguration(workspace->rootUri);
        if (config.diagnostics.includeDependents || config.diagnostics.workspace)
        {
            std::unordered_map<std::string, lsp::SingleDocumentDiagnosticReport> reverseDependencyDiagnostics;
            for (auto& module : markedDirty)
            {
                auto filePath = workspace->fileResolver.resolveToRealPath(module);
                if (filePath)
                {
                    auto uri = Uri::file(*filePath);
                    if (uri != params.textDocument.uri && !contains(diagnostics.relatedDocuments, uri.toString()))
                    {
                        auto dependencyDiags = workspace->documentDiagnostics(lsp::DocumentDiagnosticParams{uri});
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
                    client->sendNotification(
                        "textDocument/publishDiagnostics", lsp::PublishDiagnosticsParams{Uri::parse(uri), std::nullopt, diagnostics.items});
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

    // If this was an ignored file then lets clear the diagnostics for it
    if (workspace->isIgnoredFile(params.textDocument.uri.fsPath()))
    {
        client->sendNotification("textDocument/publishDiagnostics", lsp::PublishDiagnosticsParams{params.textDocument.uri, std::nullopt, {}});
    }
}

void LanguageServer::onDidChangeConfiguration(const lsp::DidChangeConfigurationParams& params)
{
    // We can't tell what workspace this is for, so we will have to clear our config information and
    // manually get it for all the workspaces again
    if (client->capabilities.workspace && client->capabilities.workspace->configuration)
    {
        client->configStore.clear();

        // Send off requests to get the configuration again for each workspace
        std::vector<lsp::DocumentUri> items;
        for (auto& workspace : workspaceFolders)
        {
            items.emplace_back(workspace->rootUri);
        }
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
    std::vector<lsp::DocumentUri> configItems;
    for (auto& folder : params.event.added)
    {
        workspaceFolders.emplace_back(std::make_shared<WorkspaceFolder>(client, folder.name, folder.uri));
        configItems.emplace_back(folder.uri);
    }
    client->requestConfiguration(configItems);
}

void LanguageServer::onDidChangeWatchedFiles(const lsp::DidChangeWatchedFilesParams& params)
{
    for (const auto& change : params.changes)
    {
        auto workspace = findWorkspace(change.uri);
        auto filePath = change.uri.fsPath();
        // Flag sourcemap changes
        if (filePath.filename() == "sourcemap.json")
        {
            client->sendLogMessage(lsp::MessageType::Info, "Registering sourcemap changed for workspace " + workspace->name);
            workspace->updateSourceMap();
        }
        else if (filePath.filename() == ".luaurc")
        {
            client->sendLogMessage(
                lsp::MessageType::Info, "Acknowledge config changed for workspace " + workspace->name + ", clearing configuration cache");
            workspace->fileResolver.clearConfigCache();

            // Send client request to refresh diagnostics
            client->refreshWorkspaceDiagnostics();
        }
    }
}

std::vector<lsp::CompletionItem> LanguageServer::completion(const lsp::CompletionParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->completion(params);
}

std::vector<lsp::DocumentLink> LanguageServer::documentLink(const lsp::DocumentLinkParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->documentLink(params);
}

// TODO: can't type this as lsp::hover as it can return null
Response LanguageServer::hover(const lsp::HoverParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    if (auto result = workspace->hover(params))
        return *result;
    return nullptr;
}

// TODO: can't type this as lsp::SignatureHelp as it can return null
Response LanguageServer::signatureHelp(const lsp::SignatureHelpParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    if (auto result = workspace->signatureHelp(params))
        return *result;
    return nullptr;
}

Response LanguageServer::gotoDefinition(const lsp::DefinitionParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    auto result = workspace->gotoDefinition(params);
    if (result)
        return *result;
    return nullptr;
}

Response LanguageServer::gotoTypeDefinition(const lsp::TypeDefinitionParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    auto result = workspace->gotoTypeDefinition(params);
    if (result)
        return *result;
    return nullptr;
}

Response LanguageServer::references(const lsp::ReferenceParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    auto result = workspace->references(params);
    if (result)
        return *result;
    return nullptr;
}

Response LanguageServer::rename(const lsp::RenameParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    auto result = workspace->rename(params);
    if (result)
        return *result;
    return nullptr;
}

lsp::DocumentDiagnosticReport LanguageServer::documentDiagnostic(const lsp::DocumentDiagnosticParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->documentDiagnostics(params);
}

lsp::WorkspaceDiagnosticReport LanguageServer::workspaceDiagnostic(const lsp::WorkspaceDiagnosticParams& params)
{
    lsp::WorkspaceDiagnosticReport fullReport;

    for (auto& workspace : workspaceFolders)
    {
        auto report = workspace->workspaceDiagnostics(params);
        fullReport.items.insert(fullReport.items.end(), std::make_move_iterator(report.items.begin()), std::make_move_iterator(report.items.end()));
    }

    return fullReport;
}

Response LanguageServer::onShutdown(const id_type& id)
{
    shutdownRequested = true;
    return nullptr;
}
