#include <iostream>
#include <string>
#include <optional>
#include <variant>
#include <exception>
#include "Protocol.hpp"
#include "Luau/StringUtils.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;
using id_type = std::variant<int, std::string>;
using Response = json;

class JsonRpcException : public std::exception
{
public:
    JsonRpcException(lsp::ErrorCode code, const std::string& message) noexcept
        : code(code)
        , message(message)
        , data(nullptr)
    {
    }
    JsonRpcException(lsp::ErrorCode code, const std::string& message, const json& data) noexcept
        : code(code)
        , message(message)
        , data(data)
    {
    }

    lsp::ErrorCode code;
    std::string message;
    json data;
};

#define REQUIRED_PARAMS(params, method) \
    !params ? throw JsonRpcException(lsp::ErrorCode::InvalidParams, "params not provided for " method) : params.value()

class LanguageServer
{
public:
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
    // void sendResponse(std::optional<id_type> id, const JsonRpcException& error)
    // {
    //     // TODO
    //     json msg{
    //         {"jsonrpc", "2.0"}
    //     }
    // }
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

    lsp::ServerCapabilities getServerCapabilities()
    {
        lsp::TextDocumentSyncKind textDocumentSync = lsp::TextDocumentSyncKind::Full;
        lsp::CompletionOptions completionProvider{};
        return lsp::ServerCapabilities{textDocumentSync, completionProvider};
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
            return onInitialize(id);
        }
        else if (method == "shutdown")
        {
            return onShutdown(id);
        }
        else
        {
            throw JsonRpcException(lsp::ErrorCode::MethodNotFound, "method not found / supported");
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
    }

    void processInputLoop()
    {
        std::string jsonString;
        while (std::cin)
        {
            sendTrace(jsonString, std::nullopt);

            if (readRawMessage(jsonString))
            {
                try
                {
                    // Parse the input
                    auto j = json::parse(jsonString);
                    std::string jsonrpc_version = j.at("jsonrpc").get<std::string>();

                    if (jsonrpc_version != "2.0")
                        throw JsonRpcException(lsp::ErrorCode::ParseError, "not a json-rpc 2.0 message");

                    // Parse id - if no id, then this is a notification
                    std::optional<id_type> id;
                    if (j.contains("id"))
                    {
                        if (j.at("id").is_string())
                        {
                            id = j.at("id").get<std::string>();
                        }
                        else if (j.at("id").is_number())
                        {
                            id = j.at("id").get<int>();
                        }
                    }

                    // Parse method - if no method then this is a response
                    std::optional<std::string> method;
                    if (j.contains("method"))
                        j.at("method").get_to(method);

                    // Handle response
                    if (!method.has_value())
                    {
                        if (!id.has_value())
                            throw JsonRpcException(lsp::ErrorCode::InvalidRequest, "no id or method");

                        // TODO: check error or result
                        continue;
                    }

                    // Parse params (if present)
                    std::optional<json> params;
                    if (j.contains("params"))
                        params = j.at("params");

                    if (id.has_value())
                    {
                        auto response = onRequest(id.value(), method.value(), params);
                        sendTrace(response.dump(), std::nullopt);
                        sendResponse(id.value(), response);
                    }
                    else
                    {
                        onNotification(method.value(), params);
                    }
                }
                catch (const JsonRpcException& e)
                {
                    sendRawMessage({{"jsonrpc", "2.0"}, {"id", nullptr}, // TODO: id
                        {"error", {{"code", e.code}, {"message", e.message}, {"data", e.data}}}});
                }
                catch (const json::exception& e)
                {
                    sendRawMessage({{"jsonrpc", "2.0"}, {"id", nullptr}, // TODO: id
                        {"error", {"code", lsp::ErrorCode::ParseError}, {"message", e.what()}}});
                }
                catch (const std::exception& e)
                {
                    sendRawMessage({{"jsonrpc", "2.0"}, {"id", nullptr}, // TODO: id
                        {"error", {"code", lsp::ErrorCode::InternalError}, {"message", e.what()}}});
                }
            }
        }
    }

    bool requestedShutdown()
    {
        return shutdownRequested;
    }

    // Dispatch handlers
    lsp::InitializeResult onInitialize(const id_type& id) // const lsp::InitializeParams& params
    {
        isInitialized = true;
        lsp::InitializeResult result;
        result.capabilities = getServerCapabilities();
        return result;
    }

    void onInitialized(const lsp::InitializedParams& params)
    {
        // Client received result of initialize
        sendLogMessage(lsp::MessageType::Info, "server initialized!");
    }

    void onDidOpenTextDocument(const lsp::DidOpenTextDocumentParams& params)
    {
        sendLogMessage(lsp::MessageType::Info, "got an opened file");
        // TODO: manage file location
        // TODO: trigger diagnostics
    }

    void onDidChangeTextDocument(const lsp::DidChangeTextDocumentParams& params)
    {
        sendLogMessage(lsp::MessageType::Info, "got a changed file");
        // TODO: update local version of file
        // TODO: trigger diagnostics
    }

    void onDidCloseTextDocument(const lsp::DidCloseTextDocumentParams& params)
    {
        sendLogMessage(lsp::MessageType::Info, "got a closed file");
        // TODO: release local version of file
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

    /** Reads stdin for a JSON-RPC message into output */
    bool readRawMessage(std::string& output)
    {
        unsigned int contentLength = 0;
        std::string line;

        // Read the headers
        while (true)
        {
            if (!std::cin)
                return false;
            std::getline(std::cin, line);

            if (Luau::startsWith(line, "Content-Length: "))
            {
                if (contentLength != 0)
                {
                    std::cerr << "Duplicate content-length header found. Discarding old value";
                }
                std::string len = line.substr(16);
                contentLength = std::stoi(len);
                continue;
            }

            // Trim line and check if its empty (i.e., we have ended the header block)
            line.erase(line.find_last_not_of(" \n\r\t") + 1);
            if (line.empty())
                break;
        }

        // Check if no Content-Length found
        if (contentLength == 0)
        {
            std::cerr << "Failed to read content length\n";
            return false;
        }

        // TODO: check if contentlength is too large?

        // Read the JSON message into output
        output.resize(contentLength);
        std::cin.read(&output[0], contentLength);
        return true;
    }

    /** Sends a raw JSON-RPC message to stdout */
    void sendRawMessage(const json& message)
    {
        std::string s = message.dump();
        std::cout << "Content-Length: " << s.length() << '\n'; // TODO: these should be '\r\n' (SO MUCH DEBUGGING PAIN - APPARENTLY WINDOWS AUTO
                                                               // CONVERTS \n TO \r\n, BUT THEN YOU ACTUALLY OUTPUT \r\r\n?????)
        std::cout << '\n';
        std::cout << s;
        std::cout.flush();
    }
};

int main()
{
    LanguageServer server;

    // Debug loop: uncomment and set a breakpoint on while to attach debugger before init
    // auto d = 4;
    // while (d == 4)
    // {
    //     d = 4;
    // }

    // Begin input loop
    server.processInputLoop();

    // If we received a shutdown request before exiting, exit normally. Otherwise, it is an abnormal exit
    return server.requestedShutdown() ? 0 : 1;
}