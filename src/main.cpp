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
    std::cout << "Content-Length: " << s.length() << '\n'; // TODO: these should be '\r\n' (SO MUCH DEBUGGING PAIN - APPARENTLY WINDOWS AUTO CONVERTS
                                                           // \n TO \r\n, BUT THEN YOU ACTUALLY OUTPUT \r\r\n?????)
    std::cout << '\n';
    std::cout << s;
    std::cout.flush();
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

// void onRequest(int id, /* TODO: can be a string too? maybe just take a JSON? */ const std::string& method /*, optional json value params*/) {}
// // void onResponse(); // id = integer/string/null, result?: string | number | boolean | object | null, error?: ResponseError
// void onNotification(const std::string& method, std::optional<const json&> params) {}

void sendLogMessage(lsp::MessageType type, std::string message)
{
    json params{
        {"type", type},
        {"message", message},
    };
    sendNotification("window/logMessage", params);
}

lsp::ServerCapabilities getServerCapabilities()
{
    lsp::CompletionOptions completionProvider{};
    return lsp::ServerCapabilities{completionProvider};
}

int main()
{
    bool isInitialized = false;
    bool shutdownRequested = false;
    lsp::TraceValue traceMode = lsp::TraceValue::Off;

    auto sendTrace = [&traceMode](std::string message, std::optional<std::string> verbose)
    {
        if (traceMode == lsp::TraceValue::Off)
            return;
        json params{{"message", message}};
        if (verbose && traceMode == lsp::TraceValue::Verbose)
            params["verbose"] = verbose.value();
        sendNotification("$/logTrace", params);
    };

    // Debug loop: uncomment and set a breakpoint on while to attach debugger before init
    // auto d = 4;
    // while (d == 4)
    // {
    //     d = 4;
    // }

    // Begin input loop
    std::string jsonString;
    while (std::cin)
    {
        if (readRawMessage(jsonString))
        {
            try
            {
                // Parse the input
                // TODO: handle invalid json
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
                    // Handle request
                    // If a request has been sent before the server is initialized, we should error
                    if (!isInitialized && method.value() != "initialize")
                        throw JsonRpcException(lsp::ErrorCode::ServerNotInitialized, "server not initialized");
                    // If we received a request after a shutdown, then we error with InvalidRequest
                    if (shutdownRequested)
                        throw JsonRpcException(lsp::ErrorCode::InvalidRequest, "server is shutting down");

                    if (method.value() == "initialize")
                    {
                        isInitialized = true;
                        lsp::InitializeResult result;
                        result.capabilities = getServerCapabilities();
                        sendResponse(id.value(), result);
                    }
                    else if (method.value() == "shutdown")
                    {
                        shutdownRequested = true;
                        sendResponse(id.value(), nullptr);
                    }
                }
                else
                {
                    // Handle notification
                    // If a notification is sent before the server is initilized or after a shutdown is requested (unless its exit), we should drop it
                    if ((!isInitialized || shutdownRequested) && method.value() != "exit")
                        continue;

                    if (method.value() == "exit")
                    {
                        // Exit the process loop
                        break;
                    }
                    else if (method.value() == "initialized")
                    {
                        // Client received result of initialize
                        sendLogMessage(lsp::MessageType::Info, "server initialized!");
                    }
                    else if (method.value() == "$/setTrace")
                    {
                        if (!params)
                            throw JsonRpcException(lsp::ErrorCode::InvalidParams, "params not provided for setTrace");
                        lsp::SetTraceParams p;
                        params->get_to(p);
                        traceMode = p.value;
                    }
                }

                // TODO: dispatch to relevant handler and receive response
                // TODO: send response to client
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

    // If we received a shutdown request before exiting, exit normally. Otherwise, it is an abnormal exit
    return shutdownRequested ? 0 : 1;
}