#include "TestClient.h"

#include "LSP/Transport/StdioTransport.hpp"

TestClient::TestClient()
    : Client(std::make_unique<StdioTransport>())
{
}

void TestClient::sendRequest(const json_rpc::id_type& id, const std::string& method, const std::optional<json>& params, const std::optional<ResponseHandler>& handler)
{
    requestQueue.push_back(std::make_pair(method, params));
}

void TestClient::sendNotification(const std::string& method, const std::optional<json>& params) const
{
    notificationQueue.push_back(std::make_pair(method, params));
}