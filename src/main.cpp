#include <iostream>
#include <fstream>
#include <string>
#include <optional>
#include <variant>
#include <exception>
#include <filesystem>
#include <algorithm>
#include "LSP/Client.hpp"
#include "LSP/Protocol.hpp"
#include "LSP/JsonRpc.hpp"
#include "LSP/Uri.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/DocumentationParser.hpp"
#include "Luau/Frontend.h"
#include "Luau/Autocomplete.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/StringUtils.h"
#include "Luau/ToString.h"
#include "nlohmann/json.hpp"

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

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

class LanguageServer
{
public:
    // A "in memory" workspace folder which doesn't actually have a root.
    // Any files which aren't part of a workspace but are opened will be handled here.
    // This is common if the client has not yet opened a folder
    WorkspaceFolderPtr nullWorkspace;
    std::vector<WorkspaceFolderPtr> workspaceFolders;
    ClientPtr client;

    LanguageServer(std::optional<std::filesystem::path> definitionsFile, std::optional<std::filesystem::path> documentationFile)
        : client(std::make_shared<Client>())
    {
        client->definitionsFile = definitionsFile;
        client->documentationFile = documentationFile;
        parseDocumentation(documentationFile, client->documentation, client);
        nullWorkspace = std::make_shared<WorkspaceFolder>(client, "$NULL_WORKSPACE", Uri());
    }

    /// Finds the workspace which the file belongs to.
    /// If no workspace is found, the file is attached to the null workspace
    WorkspaceFolderPtr findWorkspace(const lsp::DocumentUri file)
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

    lsp::ServerCapabilities getServerCapabilities()
    {
        lsp::TextDocumentSyncKind textDocumentSync = lsp::TextDocumentSyncKind::Incremental;
        // Completion
        std::vector<std::string> completionTriggerCharacters{".", ":", "'", "\"", "\n"}; // \n is used to trigger end completion
        lsp::CompletionOptions::CompletionItem completionItem{/* labelDetailsSupport: */ true};
        lsp::CompletionOptions completionProvider{completionTriggerCharacters, std::nullopt, /* resolveProvider: */ false, completionItem};
        // Hover Provider
        bool hoverProvider = true;
        // Signature Help
        std::vector<std::string> signatureHelpTriggerCharacters{"(", ","};
        lsp::SignatureHelpOptions signatureHelpProvider{signatureHelpTriggerCharacters};
        // Go To Declaration Provider
        bool declarationProvider = false; // TODO: does this apply to Luau?
        // Go To Definition Provider
        bool definitionProvider = true;
        // Go To Type Definition Provider
        bool typeDefinitionProvider = true;
        // Go To Implementation Provider
        bool implementationProvider = false; // TODO: does this apply to Luau?
        // Find References Provider
        bool referencesProvider = false;
        // Document Symbol Provider
        bool documentSymbolProvider = false;
        // Document Link Provider
        lsp::DocumentLinkOptions documentLinkProvider{false};
        // Diagnostics Provider
        lsp::DiagnosticOptions diagnosticsProvider{"luau", /* interFileDependencies: */ true, /* workspaceDiagnostics: */ false};
        // Workspaces
        lsp::WorkspaceCapabilities workspace;
        lsp::WorkspaceFoldersServerCapabilities workspaceFolderCapabilities{true, false};
        workspace.workspaceFolders = workspaceFolderCapabilities;
        return lsp::ServerCapabilities{textDocumentSync, completionProvider, hoverProvider, signatureHelpProvider, declarationProvider,
            definitionProvider, typeDefinitionProvider, implementationProvider, referencesProvider, documentSymbolProvider, documentLinkProvider,
            diagnosticsProvider, workspace};
    }

    Response onRequest(const id_type& id, const std::string& method, std::optional<json> params)
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
        // else if (method == "textDocument/documentSymbol")
        // {
        //     return documentSymbol(REQUIRED_PARAMS(params, "textDocument/documentSymbol"));
        // }
        else if (method == "textDocument/diagnostic")
        {
            return documentDiagnostic(REQUIRED_PARAMS(params, "textDocument/diagnostic"));
        }
        // TODO: workspace/diagnostic?
        else
        {
            throw JsonRpcException(lsp::ErrorCode::MethodNotFound, "method not found / supported: " + method);
        }
    }

    // // void onResponse(); // id = integer/string/null, result?: string | number | boolean | object | null, error?: ResponseError
    void onNotification(const std::string& method, std::optional<json> params)
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

    void processInputLoop()
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

    bool requestedShutdown()
    {
        return shutdownRequested;
    }

    // Dispatch handlers
    lsp::InitializeResult onInitialize(const lsp::InitializeParams& params)
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

    void onInitialized(const lsp::InitializedParams& params)
    {
        // Client received result of initialize
        client->sendLogMessage(lsp::MessageType::Info, "server initialized!");
        client->sendLogMessage(lsp::MessageType::Info, "trace level: " + json(client->traceMode).dump());

        // Dynamically register for configuration changed notifications
        if (client->capabilities.workspace && client->capabilities.workspace->didChangeConfiguration &&
            client->capabilities.workspace->didChangeConfiguration->dynamicRegistration)
        {
            client->registerCapability("workspace/didChangeConfiguration", nullptr);
            // Send off requests to get the configuration for each workspace
            std::vector<lsp::DocumentUri> items;
            for (auto& workspace : workspaceFolders)
            {
                items.emplace_back(workspace->rootUri);
            }
            client->requestConfiguration(items);
        }

        // Dynamically register file watchers. Currently doing on client
        // lsp::FileSystemWatcher watcher{"sourcemap.json"};
        // registerCapability("WORKSPACE-FILE-WATCHERS", "workspace/didChangeWatchedFiles", lsp::DidChangeWatchedFilesRegistrationOptions{{watcher}});
    }

    void pushDiagnostics(WorkspaceFolderPtr& workspace, const lsp::DocumentUri& uri, const int version)
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

    void onDidOpenTextDocument(const lsp::DidOpenTextDocumentParams& params)
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

    void onDidChangeTextDocument(const lsp::DidChangeTextDocumentParams& params)
    {
        // Update in-memory file with new contents
        auto workspace = findWorkspace(params.textDocument.uri);
        workspace->updateTextDocument(params.textDocument.uri, params);

        // Trigger diagnostics
        // By default, we rely on the pull based diagnostics model (based on documentDiagnostic)
        // however if a client doesn't yet support it, we push the diagnostics instead
        if (!client->capabilities.textDocument || !client->capabilities.textDocument->diagnostic)
        {
            // Convert the diagnostics report into a series of diagnostics published for each relevant file
            auto diagnostics = workspace->documentDiagnostics(lsp::DocumentDiagnosticParams{params.textDocument});
            client->sendNotification("textDocument/publishDiagnostics",
                lsp::PublishDiagnosticsParams{params.textDocument.uri, params.textDocument.version, diagnostics.items});

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

    void onDidCloseTextDocument(const lsp::DidCloseTextDocumentParams& params)
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

    void onDidChangeConfiguration(const lsp::DidChangeConfigurationParams& params)
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

    void onDidChangeWorkspaceFolders(const lsp::DidChangeWorkspaceFoldersParams& params)
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

    void onDidChangeWatchedFiles(const lsp::DidChangeWatchedFilesParams& params)
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
        }
    }

    std::vector<lsp::CompletionItem> completion(const lsp::CompletionParams& params)
    {
        auto workspace = findWorkspace(params.textDocument.uri);
        return workspace->completion(params);
    }

    std::vector<lsp::DocumentLink> documentLink(const lsp::DocumentLinkParams& params)
    {
        auto workspace = findWorkspace(params.textDocument.uri);
        return workspace->documentLink(params);
    }

    // TODO: can't type this as lsp::hover as it can return null
    Response hover(const lsp::HoverParams& params)
    {
        auto workspace = findWorkspace(params.textDocument.uri);
        if (auto result = workspace->hover(params))
            return *result;
        return nullptr;
    }

    // TODO: can't type this as lsp::SignatureHelp as it can return null
    Response signatureHelp(const lsp::SignatureHelpParams& params)
    {
        auto workspace = findWorkspace(params.textDocument.uri);
        if (auto result = workspace->signatureHelp(params))
            return *result;
        return nullptr;
    }

    Response gotoDefinition(const lsp::DefinitionParams& params)
    {
        auto workspace = findWorkspace(params.textDocument.uri);
        auto result = workspace->gotoDefinition(params);
        if (result)
            return *result;
        return nullptr;
    }

    Response gotoTypeDefinition(const lsp::TypeDefinitionParams& params)
    {
        auto workspace = findWorkspace(params.textDocument.uri);
        auto result = workspace->gotoTypeDefinition(params);
        if (result)
            return *result;
        return nullptr;
    }

    // Response documentSymbol(const lsp::DocumentSymbolParams& params)
    // {
    //     auto workspace = findWorkspace(params.textDocument.uri);
    //     auto result = workspace->documentSymbol(params);
    //     if (result)
    //         return *result;
    //     return nullptr;
    // }

    lsp::DocumentDiagnosticReport documentDiagnostic(const lsp::DocumentDiagnosticParams& params)
    {
        auto workspace = findWorkspace(params.textDocument.uri);
        return workspace->documentDiagnostics(params);
    }

    Response onShutdown(const id_type& id)
    {
        shutdownRequested = true;
        return nullptr;
    }

private:
    bool isInitialized = false;
    bool shutdownRequested = false;
};

int main(int argc, char** argv)
{
    // Debug loop: uncomment and set a breakpoint on while to attach debugger before init
    // auto d = 4;
    // while (d == 4)
    // {
    //     d = 4;
    // }

    Luau::assertHandler() = [](const char* expr, const char* file, int line, const char*) -> int
    {
        fprintf(stderr, "%s(%d): ASSERTION FAILED: %s\n", file, line, expr);
        return 1;
    };

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    // Enable all flags
    for (Luau::FValue<bool>* flag = Luau::FValue<bool>::list; flag; flag = flag->next)
        if (strncmp(flag->name, "Luau", 4) == 0)
            flag->value = true;

    // Check passed arguments
    std::optional<std::filesystem::path> definitionsFile;
    std::optional<std::filesystem::path> documentationFile;
    for (int i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--definitions=", 14) == 0)
        {
            definitionsFile = std::filesystem::path(argv[i] + 14);
        }
        else if (strncmp(argv[i], "--docs=", 7) == 0)
        {
            documentationFile = std::filesystem::path(argv[i] + 7);
        }
    }

    LanguageServer server(definitionsFile, documentationFile);

    // Begin input loop
    server.processInputLoop();

    // If we received a shutdown request before exiting, exit normally. Otherwise, it is an abnormal exit
    return server.requestedShutdown() ? 0 : 1;
}