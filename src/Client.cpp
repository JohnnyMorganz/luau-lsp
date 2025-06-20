#include "LSP/Client.hpp"
#include "LSP/Transport/StdioTransport.hpp"
#include "Protocol/WorkDoneProgress.hpp"

#include <iostream>
#include <optional>

Client::Client()
    : transport(std::make_unique<StdioTransport>())
{
}

Client::Client(std::unique_ptr<Transport> transport)
    : transport(std::move(transport))
{
}

void Client::sendRequest(
    const id_type& id, const std::string& method, const std::optional<json>& params, const std::optional<ResponseHandler>& handler)
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

void Client::sendResponse(const id_type& id, const json& result)
{
    json msg{
        {"jsonrpc", "2.0"},
        {"result", result},
        {"id", id},
    };

    sendRawMessage(msg);
}

void Client::sendError(const std::optional<id_type>& id, const JsonRpcException& e)
{
    json msg{
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {{"code", e.code}, {"message", e.message}, {"data", e.data}}},
    };

    sendRawMessage(msg);
}

void Client::sendNotification(const std::string& method, const std::optional<json>& params) const
{
    json msg{
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params},
    };

    sendRawMessage(msg);
}

static bool supportsWorkDoneProgress(const lsp::ClientCapabilities& capabilities)
{
    return capabilities.window && capabilities.window->workDoneProgress;
}

void Client::createWorkDoneProgress(const lsp::ProgressToken& token)
{
    if (!supportsWorkDoneProgress(capabilities))
        return;

    // TODO: handle responses?
    sendRequest(nextRequestId++, "window/workDoneProgress/create", lsp::WorkDoneProgressCreateParams{token});
}

void Client::sendWorkDoneProgressBegin(
    const lsp::ProgressToken& token, const std::string& title, std::optional<std::string> message, std::optional<uint8_t> percentage)
{
    if (!supportsWorkDoneProgress(capabilities))
        return;

    lsp::WorkDoneProgressBegin workDone;
    workDone.title = title;
    workDone.cancellable = false;
    workDone.message = message;
    workDone.percentage = percentage;

    sendProgress(lsp::ProgressParams{token, workDone});
}

void Client::sendWorkDoneProgressReport(const lsp::ProgressToken& token, std::optional<std::string> message, std::optional<uint8_t> percentage)
{
    if (!supportsWorkDoneProgress(capabilities))
        return;

    lsp::WorkDoneProgressReport workDone;
    workDone.cancellable = false;
    workDone.message = message;
    workDone.percentage = percentage;

    sendProgress(lsp::ProgressParams{token, workDone});
}

void Client::sendWorkDoneProgressEnd(const lsp::ProgressToken& token, std::optional<std::string> message)
{
    if (!supportsWorkDoneProgress(capabilities))
        return;

    lsp::WorkDoneProgressEnd workDone;
    workDone.message = message;

    sendProgress(lsp::ProgressParams{token, workDone});
}

void Client::sendLogMessage(const lsp::MessageType& type, const std::string& message) const
{
    json params{
        {"type", type},
        {"message", message},
    };
    sendNotification("window/logMessage", params);
}

void Client::sendTrace(const std::string& message, const std::optional<std::string>& verbose) const
{
    if (traceMode == lsp::TraceValue::Off)
        return;
    json params{{"message", message}};
    if (verbose && traceMode == lsp::TraceValue::Verbose)
        params["verbose"] = verbose.value();
    sendNotification("$/logTrace", params);
}

void Client::sendWindowMessage(const lsp::MessageType& type, const std::string& message) const
{
    lsp::ShowMessageParams params{type, message};
    sendNotification("window/showMessage", params);
}

void Client::registerCapability(const std::string& registrationId, const std::string& method, const json& registerOptions)
{
    lsp::Registration registration{registrationId, method, registerOptions};
    // TODO: handle responses?
    sendRequest(nextRequestId++, "client/registerCapability", lsp::RegistrationParams{{registration}});
}

void Client::unregisterCapability(const std::string& registrationId, const std::string& method)
{
    lsp::Unregistration unregistration{registrationId, method};
    // TODO: handle responses?
    sendRequest(nextRequestId++, "client/unregisterCapability", lsp::UnregistrationParams{{unregistration}});
}

ClientConfiguration Client::getConfiguration(const lsp::DocumentUri& uri)
{
    if (configStore.find(uri) != configStore.end())
        return configStore[uri];
    return globalConfig;
}

void Client::removeConfiguration(const lsp::DocumentUri& uri)
{
    configStore.erase(uri);
}

void Client::requestConfiguration(const std::vector<lsp::DocumentUri>& uris)
{
    std::vector<lsp::ConfigurationItem> items{};
    for (auto& uri : uris)
    {
        if (uri == Uri()) // Handle null workspace (global config)
            items.emplace_back(lsp::ConfigurationItem{std::nullopt, "luau-lsp"});
        else
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
                    ClientConfiguration config;
                    if (!configIt->is_null())
                        config = *configIt;

                    ClientConfiguration* oldConfig = nullptr;
                    if (auto it = configStore.find(uri); it != configStore.end())
                        oldConfig = &it->second;

                    configStore.insert_or_assign(uri, config);
                    sendLogMessage(lsp::MessageType::Info, "loaded configuration for " + uri.toString());
                    if (configChangedCallback)
                        configChangedCallback(uri, config, oldConfig);
                    ++workspaceIt;
                    ++configIt;
                }

                // See if we have some remainders
                if (workspaceIt != uris.end() || configIt != configs.end())
                {
                    sendLogMessage(lsp::MessageType::Warning, "erroneous config provided");
                }
            }
        }
    };

    sendRequest(nextRequestId++, "workspace/configuration", lsp::ConfigurationParams{items}, handler);
}

void Client::applyEdit(const lsp::ApplyWorkspaceEditParams& params, const std::optional<ResponseHandler>& handler)
{
    sendRequest(nextRequestId++, "workspace/applyEdit", params, handler);
}

void Client::publishDiagnostics(const lsp::PublishDiagnosticsParams& params)
{
    sendNotification("textDocument/publishDiagnostics", params);
}

void Client::refreshWorkspaceDiagnostics()
{
    if (capabilities.workspace && capabilities.workspace->diagnostics && capabilities.workspace->diagnostics->refreshSupport)
    {
        sendRequest(nextRequestId++, "workspace/diagnostic/refresh", nullptr);
    }
}

void Client::refreshInlayHints()
{
    if (capabilities.workspace && capabilities.workspace->inlayHint && capabilities.workspace->inlayHint->refreshSupport)
        sendRequest(nextRequestId++, "workspace/inlayHint/refresh", nullptr);
}

void Client::setTrace(const lsp::SetTraceParams& params)
{
    traceMode = params.value;
}

bool Client::readRawMessage(std::string& output) const
{
    return json_rpc::readRawMessage(transport.get(), output);
}

void Client::handleResponse(const JsonRpcMessage& message)
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

void Client::sendRawMessage(const json& message) const
{
    json_rpc::sendRawMessage(transport.get(), message);
}
