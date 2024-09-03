#include "LSP/ServerIO.hpp"
#include "LSP/JsonRpc.hpp"
#include "Protocol/Lifecycle.hpp"
#include "Protocol/ClientCapabilities.hpp"
#include "Protocol/Structures.hpp"
#include "Protocol/Diagnostics.hpp"
#include "Protocol/Window.hpp"
#include "Protocol/Workspace.hpp"

using ResponseHandler = std::function<void(const JsonRpcMessage&)>;

const void ServerIO::sendResponse(const id_type& id, const json& result) const
{
    json msg{
        {"jsonrpc", "2.0"},
        {"result", result},
        {"id", id},
    };

    sendRawMessage(msg);
}

const void ServerIO::sendError(const std::optional<id_type>& id, const JsonRpcException& e)
{
    json msg{
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {{"code", e.code}, {"message", e.message}, {"data", e.data}}},
    };

    sendRawMessage(msg);
}

const void ServerIO::sendNotification(const std::string& method, const std::optional<json>& params)
{
    json msg{
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params},
    };

    sendRawMessage(msg);
}

const void ServerIO::sendLogMessage(const lsp::MessageType& type, const std::string& message)
{
    json params{
        {"type", type},
        {"message", message},
    };
    sendNotification("window/logMessage", params);
}

const void ServerIO::sendWindowMessage(const lsp::MessageType& type, const std::string& message)
{
    lsp::ShowMessageParams params{type, message};
    sendNotification("window/showMessage", params);
}