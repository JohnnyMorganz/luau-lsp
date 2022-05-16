#pragma once
#include <iostream>
#include <optional>
#include "JsonRpc.hpp"
#include "Protocol.hpp"
#include "Luau/Documentation.h"

using namespace json_rpc;

class Client
{
public:
    lsp::ClientCapabilities capabilities;
    lsp::TraceValue traceMode = lsp::TraceValue::Off;
    /// A registered definitions file passed by the client
    std::optional<std::filesystem::path> definitionsFile = std::nullopt;
    /// A registered documentation file passed by the client
    std::optional<std::filesystem::path> documentationFile = std::nullopt;
    /// Parsed documentation database
    Luau::DocumentationDatabase documentation{""};

    void sendRequest(const id_type& id, const std::string& method, std::optional<json> params)
    {
        json msg{
            {"jsonrpc", "2.0"},
            {"method", method},
            {"id", id},
            {"params", params},
        };

        sendRawMessage(msg);
    }

    void sendResponse(const id_type& id, const json& result)
    {
        json msg{
            {"jsonrpc", "2.0"},
            {"result", result},
            {"id", id},
        };

        sendRawMessage(msg);
    }

    void sendError(const std::optional<id_type>& id, const JsonRpcException& e)
    {
        json msg{
            {"jsonrpc", "2.0"},
            {"id", id},
            {"error", {{"code", e.code}, {"message", e.message}, {"data", e.data}}},
        };

        sendRawMessage(msg);
    }

    void sendNotification(const std::string& method, std::optional<json> params)
    {
        json msg{
            {"jsonrpc", "2.0"},
            {"method", method},
            {"params", params},
        };

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

    void sendTrace(std::string message, std::optional<std::string> verbose = std::nullopt)
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

    void setTrace(const lsp::SetTraceParams& params)
    {
        traceMode = params.value;
    }

    bool readRawMessage(std::string& output)
    {
        return json_rpc::readRawMessage(std::cin, output);
    }

private:
    void sendRawMessage(const json& message)
    {
        json_rpc::sendRawMessage(std::cout, message);
    }
};