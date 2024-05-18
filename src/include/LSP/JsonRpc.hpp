#pragma once
#include <exception>
#include <variant>
#include <iostream>
#include "Luau/StringUtils.h"
#include "nlohmann/json.hpp"
#include "Protocol/Base.hpp"
#include "LSP/Utils.hpp"

using json = nlohmann::json;

namespace json_rpc
{
using id_type = std::variant<std::string, int>;
using Response = json;

class JsonRpcException : public std::exception
{
public:
    JsonRpcException(lsp::ErrorCode code, std::string message) noexcept
        : code(code)
        , message(std::move(message))
        , data(nullptr)
    {
    }
    JsonRpcException(lsp::ErrorCode code, std::string message, json data) noexcept
        : code(code)
        , message(std::move(message))
        , data(std::move(data))
    {
    }

    lsp::ErrorCode code;
    std::string message;
    json data;

    const char* what() const noexcept override
    {
        return message.c_str();
    }
};

class JsonRpcMessage
{
public:
    std::optional<id_type> id;
    std::optional<std::string> method;
    std::optional<json> params;
    std::optional<json> result;
    std::optional<JsonRpcException> error;

    [[nodiscard]] bool is_request() const
    {
        return this->id.has_value() && this->method.has_value();
    }

    [[nodiscard]] bool is_response() const
    {
        return this->id.has_value() && (this->result.has_value() || this->error.has_value());
    }

    [[nodiscard]] bool is_notification() const
    {
        return !this->id.has_value() && this->method.has_value();
    }
};

JsonRpcMessage parse(const std::string& jsonString);

/// Reads a JSON-RPC message from input
bool readRawMessage(std::istream& input, std::string& output);

/// Sends a raw JSON-RPC message to output stream
void sendRawMessage(std::ostream& output, const json& message);

} // namespace json_rpc
