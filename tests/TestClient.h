#pragma once

#include "LSP/Client.hpp"

#include <vector>

class TestClient : public Client
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
};
