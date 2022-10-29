#include "LSP/JsonRpc.hpp"

#include <exception>
#include <variant>

#include "nlohmann/json.hpp"
#include "Luau/StringUtils.h"
#include "LSP/Utils.hpp"

using json = nlohmann::json;

namespace json_rpc
{
JsonRpcMessage parse(const std::string& jsonString)
{
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

    // Parse params (if present)
    std::optional<json> params;
    if (j.contains("params"))
        params = j.at("params");

    // Parse result if present
    std::optional<json> result;
    if (j.contains("result"))
        result = j.at("result");

    // Parse error if present
    std::optional<JsonRpcException> error;
    if (j.contains("error"))
    {
        auto err = j.at("error");
        auto code = err.at("code").get<lsp::ErrorCode>();
        auto message = err.at("message").get<std::string>();
        if (err.contains("data"))
        {
            error = JsonRpcException(code, message, err.at("data"));
        }
        else
        {
            error = JsonRpcException(code, message);
        }
    }

    return JsonRpcMessage{id, method, params, result, error};
}

/// Reads a JSON-RPC message from input
bool readRawMessage(std::istream& input, std::string& output)
{
    unsigned int contentLength = 0;
    std::string line;

    // Read the headers
    while (true)
    {
        if (!input)
            return false;
        std::getline(input, line);

        if (Luau::startsWith(line, "Content-Length: "))
        {
            if (contentLength != 0)
            {
                std::cerr << "Duplicate content-length header found. Discarding old value";
            }
            std::string len = line.substr(16);
            trim_end(len);
            contentLength = std::stoi(len);
            continue;
        }

        // Trim line and check if its empty (i.e., we have ended the header block)
        trim_end(line);
        if (line.empty())
            break;
    }

    // Check if no Content-Length found
    if (contentLength == 0)
    {
        return false;
    }

    // TODO: check if contentlength is too large?

    // Read the JSON message into output
    output.resize(contentLength);
    input.read(&output[0], contentLength);
    return true;
}

/// Sends a raw JSON-RPC message to output stream
void sendRawMessage(std::ostream& output, const json& message)
{
    std::string s = message.dump();
    output << "Content-Length: " << s.length() << "\r\n";
    output << "\r\n";
    output << s;
    output.flush();
}

} // namespace json_rpc