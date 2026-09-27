#pragma once

#include "LSP/Client.hpp"
#include "Protocol/Lifecycle.hpp"

#include <vector>

class TestClient : public LSPClient
{
public:
    TestClient();

    std::vector<std::pair<std::string, std::optional<json>>> requestQueue;
    mutable std::vector<std::pair<std::string, std::optional<json>>> notificationQueue;
    std::vector<std::pair<std::optional<id_type>, JsonRpcException>> errorQueue;

    void sendRequest(
        const id_type& id, const std::string& method, const std::optional<json>& params, const std::optional<ResponseHandler>& handler) override;
    void sendNotification(const std::string& method, const std::optional<json>& params) const override;
    void sendError(const std::optional<id_type>& id, const JsonRpcException& e) override;

    /// Finds a `client/registerCapability` request in `requestQueue` that registered for the given
    /// LSP method (e.g. "textDocument/diagnostic"), if any.
    std::optional<lsp::Registration> findRegistration(const std::string& method) const;
    /// Returns whether a `client/unregisterCapability` request in `requestQueue` unregistered the
    /// given LSP method.
    bool hasUnregistration(const std::string& method) const;
};
