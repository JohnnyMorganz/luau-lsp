#pragma once
#include <optional>
#include "Luau/Documentation.h"
#include "Protocol/Lifecycle.hpp"
#include "Protocol/ClientCapabilities.hpp"
#include "Protocol/Structures.hpp"
#include "Protocol/Diagnostics.hpp"
#include "Protocol/Window.hpp"
#include "Protocol/Workspace.hpp"
#include "LSP/JsonRpc.hpp"
#include <exception>

using namespace json_rpc;

class ServerIO {
    public:
    ServerIO() {}
    virtual ~ServerIO() = default;
    virtual const bool readRawMessage(std::string& output) const = 0;
    virtual const void sendRawMessage(const json& message) const = 0;

    const void sendResponse(const id_type& id, const json& result) const
    {
        json msg{
            {"jsonrpc", "2.0"},
            {"result", result},
            {"id", id},
        };

        sendRawMessage(msg);
    }
    const void sendError(const std::optional<id_type>& id, const JsonRpcException& e) const
    {
        json msg{
            {"jsonrpc", "2.0"},
            {"id", id},
            {"error", {{"code", e.code}, {"message", e.message}, {"data", e.data}}},
        };

        sendRawMessage(msg);
    }

    const void sendNotification(const std::string& method, const std::optional<json>& params) const
    {
        json msg{
            {"jsonrpc", "2.0"},
            {"method", method},
            {"params", params},
        };

        sendRawMessage(msg);
    }

    const void sendProgress(const lsp::ProgressParams& params) const
    {
        sendNotification("$/progress", params);
    }

    const void sendLogMessage(const lsp::MessageType& type, const std::string& message) const
    {
        json params{
            {"type", type},
            {"message", message},
        };
        sendNotification("window/logMessage", params);
    }
    const void sendWindowMessage(const lsp::MessageType& type, const std::string& message) const
    {
        lsp::ShowMessageParams params{type, message};
        sendNotification("window/showMessage", params);
    }
};

class ServerIOStd : public ServerIO {
    public:
    const void sendRawMessage(const json& message) const override
    {
        json_rpc::sendRawMessage(std::cout, message);
    }
    const bool readRawMessage(std::string& output) const override
    {
        return json_rpc::readRawMessage(std::cin, output);
    }
};