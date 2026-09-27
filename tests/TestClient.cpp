#include "TestClient.h"

#include "LSP/Transport/StdioTransport.hpp"

TestClient::TestClient()
    : LSPClient(std::make_unique<StdioTransport>())
{
}

void TestClient::sendRequest(
    const json_rpc::id_type& id, const std::string& method, const std::optional<json>& params, const std::optional<ResponseHandler>& handler)
{
    requestQueue.push_back(std::make_pair(method, params));
}

void TestClient::sendNotification(const std::string& method, const std::optional<json>& params) const
{
    notificationQueue.push_back(std::make_pair(method, params));
}

void TestClient::sendError(const std::optional<id_type>& id, const json_rpc::JsonRpcException& e)
{
    errorQueue.push_back(std::make_pair(id, e));
}

std::optional<lsp::Registration> TestClient::findRegistration(const std::string& method) const
{
    for (const auto& [requestMethod, params] : requestQueue)
    {
        if (requestMethod != "client/registerCapability" || !params)
            continue;
        lsp::RegistrationParams registrationParams = params.value();
        for (const auto& registration : registrationParams.registrations)
            if (registration.method == method)
                return registration;
    }
    return std::nullopt;
}

bool TestClient::hasUnregistration(const std::string& method) const
{
    for (const auto& [requestMethod, params] : requestQueue)
    {
        if (requestMethod != "client/unregisterCapability" || !params)
            continue;
        lsp::UnregistrationParams unregistrationParams = params.value();
        for (const auto& unregistration : unregistrationParams.unregisterations)
            if (unregistration.method == method)
                return true;
    }
    return false;
}