#pragma once
#include <iostream>
#include <optional>
#include "JsonRpc.hpp"
#include "Protocol.hpp"
#include "Luau/Documentation.h"

using namespace json_rpc;
using ResponseHandler = std::function<void(const JsonRpcMessage&)>;

// These are the passed configuration options by the client, prefixed with `luau-lsp.`
// Here we also define the default settings
struct ClientConfiguration
{
    bool autocompleteEnd = false;
    std::vector<std::string> ignoreGlobs;
    // std::unordered_map<std::string, std::variant<int, bool>> fastFlags; // TODO: add ability to configure fast flags
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientConfiguration, autocompleteEnd, ignoreGlobs);

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
    /// Global configuration. These are the default settings that we will use if we don't have the workspace stored in configStore
    ClientConfiguration globalConfig;
    /// Configuration passed from the language client. Currently we only handle configuration at the workspace level
    std::unordered_map<std::string /* DocumentUri */, ClientConfiguration> configStore;

private:
    /// The request id for the next request
    int nextRequestId = 0;
    std::unordered_map<id_type, ResponseHandler> responseHandler;

public:
    void sendRequest(const id_type& id, const std::string& method, std::optional<json> params, std::optional<ResponseHandler> handler = std::nullopt)
    {
        json msg{
            {"jsonrpc", "2.0"},
            {"method", method},
            {"id", id},
            {"params", params},
        };

        // Register a response handler
        if (handler)
            responseHandler.emplace(id, *handler);

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

    void registerCapability(std::string method, json registerOptions)
    {
        lsp::Registration registration{"TODO-I-NEED-TO-FIX-THIS", method, registerOptions}; // TODO: registration ID
        // TODO: handle responses?
        sendRequest(nextRequestId++, "client/registerCapability", lsp::RegistrationParams{{registration}});
    }

    const ClientConfiguration getConfiguration(const lsp::DocumentUri& uri)
    {
        auto key = uri.toString();
        if (configStore.find(key) != configStore.end())
        {
            return configStore.at(key);
        }
        return globalConfig;
    }

    void removeConfiguration(const lsp::DocumentUri& uri)
    {
        configStore.erase(uri.toString());
    }

    // TODO: this function only supports getting requests for workspaces
    void requestConfiguration(const std::vector<lsp::DocumentUri>& uris)
    {
        if (uris.empty())
            return;

        std::vector<lsp::ConfigurationItem> items;
        for (auto& uri : uris)
        {
            items.emplace_back(lsp::ConfigurationItem{uri, "luau-lsp"});
        }

        ResponseHandler handler = [uris, this](const JsonRpcMessage& message)
        {
            if (auto result = message.result)
            {
                if (result->is_array())
                {
                    lsp::GetConfigurationResponse configs = *result;

                    auto workspaceIt = uris.begin();
                    auto configIt = configs.begin();

                    while (workspaceIt != uris.end() && configIt != configs.end())
                    {
                        auto uri = *workspaceIt;
                        ClientConfiguration config = *configIt;
                        configStore.insert_or_assign(uri.toString(), config);
                        sendLogMessage(lsp::MessageType::Info, "loaded configuration for " + uri.toString());
                        ++workspaceIt;
                        ++configIt;
                    }

                    // See if we have some remainders
                    if (workspaceIt != uris.end() || configIt != configs.end())
                    {
                        sendLogMessage(lsp::MessageType::Warning, "erroneuous config provided");
                    }
                }
            }
        };

        sendRequest(nextRequestId++, "workspace/configuration", lsp::ConfigurationParams{items}, handler);
    }

    void setTrace(const lsp::SetTraceParams& params)
    {
        traceMode = params.value;
    }

    bool readRawMessage(std::string& output)
    {
        return json_rpc::readRawMessage(std::cin, output);
    }

    void handleResponse(const JsonRpcMessage& message)
    {
        // We run our own exception catcher here because we don't want an exception escaping
        // and then the main loop handler returning that. Since the client has sent a response
        // they no longer have knowledge about this. If we fail, we should just log it and move on.
        try
        {
            if (message.id)
            {
                // Check if a response handler was registered for this response
                if (responseHandler.find(*message.id) == responseHandler.end())
                    return;

                // Call the handler on the message
                auto handler = responseHandler.at(*message.id);
                handler(message);

                // Deregister the handler
                responseHandler.erase(*message.id);
            }
        }
        catch (const std::exception& e)
        {
            sendLogMessage(lsp::MessageType::Error, "failed to process response " + json(message.id).dump() + " - " + e.what());
        }
    }

private:
    void sendRawMessage(const json& message)
    {
        json_rpc::sendRawMessage(std::cout, message);
    }
};