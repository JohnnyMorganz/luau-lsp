#pragma once
#include <iostream>
#include <optional>
#include "JsonRpc.hpp"
#include "Protocol.hpp"

using namespace json_rpc;

class Client
{
public:
    lsp::TraceValue traceMode = lsp::TraceValue::Off;

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