#pragma once
#include <iostream>
#include <optional>
#include "Luau/Documentation.h"
#include "LSP/JsonRpc.hpp"
#include "LSP/Protocol.hpp"
#include "LSP/ClientConfiguration.hpp"

using namespace json_rpc;
using ResponseHandler = std::function<void(const JsonRpcMessage&)>;
using ConfigChangedCallback = std::function<void(const lsp::DocumentUri&, const ClientConfiguration&)>;

class Client
{
public:
    lsp::ClientCapabilities capabilities;
    lsp::TraceValue traceMode = lsp::TraceValue::Off;
    /// [DEPRECATED] A registered definitions file passed by the client
    std::vector<std::filesystem::path> definitionsFiles_DEPRECATED;
    std::unordered_map<std::string, std::filesystem::path> environments;

    /// A registered documentation file passed by the client
    std::optional<std::filesystem::path> documentationFile = std::nullopt;
    /// Parsed documentation database
    Luau::DocumentationDatabase documentation{""};
    /// Global configuration. These are the default settings that we will use if we don't have the workspace stored in configStore
    ClientConfiguration globalConfig;
    /// Configuration passed from the language client. Currently we only handle configuration at the workspace level
    std::unordered_map<std::string /* DocumentUri */, ClientConfiguration> configStore;

    ConfigChangedCallback configChangedCallback;

    // A partial result token for workspace diagnostics
    // If this is present, we can stream results
    std::optional<id_type> workspaceDiagnosticsRequestId = std::nullopt;
    std::optional<lsp::ProgressToken> workspaceDiagnosticsToken = std::nullopt;

private:
    /// The request id for the next request
    int nextRequestId = 0;
    std::unordered_map<id_type, ResponseHandler> responseHandler;

public:
    void sendRequest(const id_type& id, const std::string& method, std::optional<json> params, std::optional<ResponseHandler> handler = std::nullopt);
    void sendResponse(const id_type& id, const json& result);
    void sendError(const std::optional<id_type>& id, const JsonRpcException& e);
    void sendNotification(const std::string& method, std::optional<json> params);

    void sendProgress(const lsp::ProgressParams params)
    {
        sendNotification("$/progress", params);
    }

    void sendLogMessage(lsp::MessageType type, std::string message);
    void sendTrace(std::string message, std::optional<std::string> verbose = std::nullopt);
    void sendWindowMessage(lsp::MessageType type, std::string message);

    void registerCapability(std::string registrationId, std::string method, json registerOptions);

    const ClientConfiguration getConfiguration(const lsp::DocumentUri& uri);
    void removeConfiguration(const lsp::DocumentUri& uri);
    // TODO: this function only supports getting requests for workspaces
    void requestConfiguration(const std::vector<lsp::DocumentUri>& uris);
    void applyEdit(const lsp::ApplyWorkspaceEditParams& params, std::optional<ResponseHandler> handler = std::nullopt);
    void publishDiagnostics(const lsp::PublishDiagnosticsParams& params);
    void refreshWorkspaceDiagnostics();
    void terminateWorkspaceDiagnostics(bool retriggerRequest = true);

    void setTrace(const lsp::SetTraceParams& params);

    bool readRawMessage(std::string& output);

    void handleResponse(const JsonRpcMessage& message);

private:
    void sendRawMessage(const json& message);
};

using ClientPtr = std::shared_ptr<Client>;