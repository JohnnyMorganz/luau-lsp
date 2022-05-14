#include <iostream>
#include <fstream>
#include <string>
#include <optional>
#include <variant>
#include <exception>
#include <filesystem>
#include <algorithm>
#include "Protocol.hpp"
#include "JsonRpc.hpp"
#include "Uri.hpp"
#include "Workspace.hpp"
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
using id_type = std::variant<int, std::string>;
using Response = json;
using WorkspaceFolderPtr = std::shared_ptr<WorkspaceFolder>;

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
    // A "in memory" workspace folder which doesn't actually have a root.
    // Any files which aren't part of a workspace but are opened will be handled here.
    // This is common if the client has not yet opened a folder
    WorkspaceFolderPtr nullWorkspace;
    std::vector<WorkspaceFolderPtr> workspaceFolders;

public:
    LanguageServer()
        : nullWorkspace(std::make_shared<WorkspaceFolder>())
    {
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
        sendLogMessage(lsp::MessageType::Info, "cannot find workspace for " + file.toString());
        return nullWorkspace;
    }

    void sendRequest(const id_type& id, const std::string& method, std::optional<json> params)
    {
        json msg{
            {"jsonrpc", "2.0"},
            {"method", method},
        };

        if (std::holds_alternative<int>(id))
        {
            msg["id"] = std::get<int>(id);
        }
        else
        {
            msg["id"] = std::get<std::string>(id);
        }

        if (params.has_value())
            msg["params"] = params.value();

        sendRawMessage(msg);
    }

    void sendResponse(const id_type& id, const json& result)
    {
        json msg{
            {"jsonrpc", "2.0"},
            {"result", result},
        };

        if (std::holds_alternative<int>(id))
        {
            msg["id"] = std::get<int>(id);
        }
        else
        {
            msg["id"] = std::get<std::string>(id);
        }

        sendRawMessage(msg);
    }

    void sendError(const std::optional<id_type>& id, const JsonRpcException& e)
    {
        json msg{
            {"jsonrpc", "2.0"},
        };

        if (id)
        {
            if (std::holds_alternative<int>(*id))
            {
                msg["id"] = std::get<int>(*id);
            }
            else
            {
                msg["id"] = std::get<std::string>(*id);
            }
        }
        else
        {
            msg["id"] = nullptr;
        }

        msg["error"] = {};
        msg["error"]["code"] = e.code;
        msg["error"]["message"] = e.message;
        msg["error"]["data"] = e.data;

        sendRawMessage(msg);
    }

    void sendNotification(const std::string& method, std::optional<json> params)
    {
        json msg{
            {"jsonrpc", "2.0"},
            {"method", method},
        };

        if (params.has_value())
            msg["params"] = params.value();

        sendRawMessage(msg);
    }

    void sendLogMessage(lsp::MessageType type, std::string message)
    {
        json params{
            {"type", type},
            {"message", message},
        };
        sendNotification("window/logMessage", params);
    }

    void sendTrace(std::string message, std::optional<std::string> verbose)
    {
        if (traceMode == lsp::TraceValue::Off)
            return;
        json params{{"message", message}};
        if (verbose && traceMode == lsp::TraceValue::Verbose)
            params["verbose"] = verbose.value();
        sendNotification("$/logTrace", params);
    };

    void sendWindowMessage(lsp::MessageType type, std::string message)
    {
        lsp::ShowMessageParams params{type, message};
        sendNotification("window/showMessage", params);
    }

    void registerCapability(std::string id, std::string method, json registerOptions)
    {
        lsp::Registration registration{id, method, registerOptions};
        // TODO: handle responses?
        sendRequest(id, "client/registerCapability", lsp::RegistrationParams{{registration}}); // TODO: request id?
    }

    lsp::ServerCapabilities getServerCapabilities()
    {
        lsp::TextDocumentSyncKind textDocumentSync = lsp::TextDocumentSyncKind::Incremental;
        // Completion
        std::vector<std::string> completionTriggerCharacters{".", ":", "'", "\""};
        lsp::CompletionOptions::CompletionItem completionItem{true};
        lsp::CompletionOptions completionProvider{completionTriggerCharacters, std::nullopt, false, completionItem};
        // Hover Provider
        bool hoverProvider = true;
        // Signature Help
        std::vector<std::string> signatureHelpTriggerCharacters{"(", ","};
        lsp::SignatureHelpOptions signatureHelpProvider{signatureHelpTriggerCharacters};
        // Workspaces
        lsp::WorkspaceCapabilities workspace;
        lsp::WorkspaceFoldersServerCapabilities workspaceFolderCapabilities{true, false};
        workspace.workspaceFolders = workspaceFolderCapabilities;
        return lsp::ServerCapabilities{textDocumentSync, completionProvider, hoverProvider, signatureHelpProvider, workspace};
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
        else if (method == "textDocument/hover")
        {
            return hover(REQUIRED_PARAMS(params, "textDocument/hover"));
        }
        else if (method == "textDocument/signatureHelp")
        {
            return signatureHelp(REQUIRED_PARAMS(params, "textDocument/signatureHelp"));
        }
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
            lsp::SetTraceParams setTraceParams = REQUIRED_PARAMS(params, "$/setTrace");
            traceMode = setTraceParams.value;
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
            sendLogMessage(lsp::MessageType::Warning, "unknown notification method: " + method);
        }
    }

    void processInputLoop()
    {
        std::string jsonString;
        while (std::cin)
        {
            if (readRawMessage(jsonString))
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
                        sendResponse(msg.id.value(), response);
                    }
                    else if (msg.is_response())
                    {
                        // TODO: check error or result
                        continue;
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
                    sendError(id, e);
                }
                catch (const json::exception& e)
                {
                    sendError(id, JsonRpcException(lsp::ErrorCode::ParseError, e.what()));
                }
                catch (const std::exception& e)
                {
                    sendError(id, JsonRpcException(lsp::ErrorCode::InternalError, e.what()));
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
        sendTrace("starting initialization process", std::nullopt);

        // Set provided settings
        traceMode = params.trace;

        // Configure workspaces
        if (params.workspaceFolders.has_value())
        {
            for (auto& folder : params.workspaceFolders.value())
            {
                workspaceFolders.push_back(std::make_shared<WorkspaceFolder>(folder.name, folder.uri));
            }
        }
        else if (params.rootUri.has_value())
        {
            workspaceFolders.push_back(std::make_shared<WorkspaceFolder>("$ROOT", params.rootUri.value()));
        }

        isInitialized = true;
        lsp::InitializeResult result;
        result.capabilities = getServerCapabilities();
        return result;
    }

    void onInitialized(const lsp::InitializedParams& params)
    {
        // Client received result of initialize
        sendLogMessage(lsp::MessageType::Info, "server initialized!");
        sendLogMessage(lsp::MessageType::Info, "trace level: " + json(traceMode).dump());

        // Dynamically register file watchers. Currently doing on client
        // lsp::FileSystemWatcher watcher{"sourcemap.json"};
        // registerCapability("WORKSPACE-FILE-WATCHERS", "workspace/didChangeWatchedFiles", lsp::DidChangeWatchedFilesRegistrationOptions{{watcher}});
    }

    void onDidOpenTextDocument(const lsp::DidOpenTextDocumentParams& params)
    {
        // Start managing the file in-memory
        auto workspace = findWorkspace(params.textDocument.uri);
        workspace->openTextDocument(params.textDocument.uri, params.textDocument.text);

        // Trigger diagnostics
        auto diagnostics = workspace->publishDiagnostics(params.textDocument.uri, params.textDocument.version);
        sendNotification("textDocument/publishDiagnostics", diagnostics);
    }

    void onDidChangeTextDocument(const lsp::DidChangeTextDocumentParams& params)
    {
        // Update in-memory file with new contents
        auto workspace = findWorkspace(params.textDocument.uri);
        workspace->updateTextDocument(params.textDocument.uri, params.contentChanges);

        // Trigger diagnostics
        auto diagnostics = workspace->publishDiagnostics(params.textDocument.uri, params.textDocument.version);
        sendNotification("textDocument/publishDiagnostics", diagnostics);
    }

    void onDidCloseTextDocument(const lsp::DidCloseTextDocumentParams& params)
    {
        // Release managed in-memory file
        auto workspace = findWorkspace(params.textDocument.uri);
        workspace->closeTextDocument(params.textDocument.uri);
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
            }
            else
            {
                ++it;
            }
        }

        // Add new folders
        for (auto& folder : params.event.added)
        {
            workspaceFolders.emplace_back(std::make_shared<WorkspaceFolder>(folder.name, folder.uri));
        }
    }

    void onDidChangeWatchedFiles(const lsp::DidChangeWatchedFilesParams& params)
    {
        sendLogMessage(lsp::MessageType::Info, "got watched file change");
        for (const auto& change : params.changes)
        {
            auto workspace = findWorkspace(change.uri);
            auto filePath = change.uri.fsPath();
            // Flag sourcemap changes
            if (filePath.filename() == "sourcemap.json")
            {
                sendLogMessage(lsp::MessageType::Info, "sourcemap changed");
                workspace->updateSourceMap();
            }
        }
    }

    std::vector<lsp::CompletionItem> completion(const lsp::CompletionParams& params)
    {
        auto workspace = findWorkspace(params.textDocument.uri);
        return workspace->completion(params);
    }

    // TODO: can't type this as lsp::hover as it can return null
    Response hover(const lsp::HoverParams& params)
    {
        auto workspace = findWorkspace(params.textDocument.uri);
        auto result = workspace->hover(params);
        if (result)
            return *result;
        return nullptr;
    }

    // TODO: can't type this as lsp::SignatureHelp as it can return null
    Response signatureHelp(const lsp::SignatureHelpParams& params)
    {
        auto workspace = findWorkspace(params.textDocument.uri);
        auto result = workspace->signatureHelp(params);
        if (result)
            return *result;
        return nullptr;
    }

    Response onShutdown(const id_type& id)
    {
        shutdownRequested = true;
        return nullptr;
    }

private:
    bool isInitialized = false;
    bool shutdownRequested = false;
    lsp::TraceValue traceMode = lsp::TraceValue::Off;

    bool readRawMessage(std::string& output)
    {
        return json_rpc::readRawMessage(std::cin, output);
    }

    void sendRawMessage(const json& message)
    {
        json_rpc::sendRawMessage(std::cout, message);
    }
};

int main()
{
    // Debug loop: uncomment and set a breakpoint on while to attach debugger before init
    // auto d = 4;
    // while (d == 4)
    // {
    //     d = 4;
    // }

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    // Enable all flags
    for (Luau::FValue<bool>* flag = Luau::FValue<bool>::list; flag; flag = flag->next)
        if (strncmp(flag->name, "Luau", 4) == 0)
            flag->value = true;

    LanguageServer server;

    // Begin input loop
    server.processInputLoop();

    // If we received a shutdown request before exiting, exit normally. Otherwise, it is an abnormal exit
    return server.requestedShutdown() ? 0 : 1;
}