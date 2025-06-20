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
#include "LSP/ClientConfiguration.hpp"
#include "LSP/Transport/Transport.hpp"

using namespace json_rpc;
using ResponseHandler = std::function<void(const JsonRpcMessage&)>;
using ConfigChangedCallback = std::function<void(const lsp::DocumentUri&, const ClientConfiguration&, /* oldConfig: */ const ClientConfiguration*)>;

struct BaseClient
{
    virtual ~BaseClient() = default;

    virtual ClientConfiguration getConfiguration(const lsp::DocumentUri& uri) = 0;

    virtual void publishDiagnostics(const lsp::PublishDiagnosticsParams& params) = 0;
};

class Client : public BaseClient
{
public:
    lsp::ClientCapabilities capabilities;
    lsp::TraceValue traceMode = lsp::TraceValue::Off;
    /// A registered definitions file passed by the client
    std::vector<std::string> definitionsFiles{};
    /// A registered documentation file passed by the client
    std::vector<std::string> documentationFiles{};
    /// Parsed documentation database
    Luau::DocumentationDatabase documentation{""};
    /// Global configuration. These are the default settings that we will use if we don't have the workspace stored in configStore
    ClientConfiguration globalConfig{};
    /// Configuration passed from the language client. Currently we only handle configuration at the workspace level
    std::unordered_map<Uri, ClientConfiguration, UriHash> configStore{};

    ConfigChangedCallback configChangedCallback;

    // A partial result token for workspace diagnostics
    // If this is present, we can stream results
    std::optional<id_type> workspaceDiagnosticsRequestId = std::nullopt;
    std::optional<lsp::ProgressToken> workspaceDiagnosticsToken = std::nullopt;

private:
    std::unique_ptr<Transport> transport;
    /// The request id for the next request
    int nextRequestId = 0;
    std::unordered_map<id_type, ResponseHandler> responseHandler{};

public:
    Client();
    Client(std::unique_ptr<Transport> transport);

    virtual void sendRequest(const id_type& id, const std::string& method, const std::optional<json>& params,
        const std::optional<ResponseHandler>& handler = std::nullopt);
    void sendResponse(const id_type& id, const json& result);
    void sendError(const std::optional<id_type>& id, const JsonRpcException& e);
    virtual void sendNotification(const std::string& method, const std::optional<json>& params) const;

    void sendProgress(const lsp::ProgressParams& params)
    {
        sendNotification("$/progress", params);
    }
    void createWorkDoneProgress(const lsp::ProgressToken& token);
    void sendWorkDoneProgressBegin(const lsp::ProgressToken& token, const std::string& title, std::optional<std::string> message = std::nullopt,
        std::optional<uint8_t> percentage = std::nullopt);
    void sendWorkDoneProgressReport(
        const lsp::ProgressToken& token, std::optional<std::string> message = std::nullopt, std::optional<uint8_t> percentage = std::nullopt);
    void sendWorkDoneProgressEnd(const lsp::ProgressToken& token, std::optional<std::string> message = std::nullopt);

    void sendLogMessage(const lsp::MessageType& type, const std::string& message) const;
    void sendTrace(const std::string& message, const std::optional<std::string>& verbose = std::nullopt) const;
    void sendWindowMessage(const lsp::MessageType& type, const std::string& message) const;

    void registerCapability(const std::string& registrationId, const std::string& method, const json& registerOptions);
    void unregisterCapability(const std::string& registrationId, const std::string& method);

    ClientConfiguration getConfiguration(const lsp::DocumentUri& uri) override;
    void removeConfiguration(const lsp::DocumentUri& uri);
    // TODO: this function only supports getting requests for workspaces
    void requestConfiguration(const std::vector<lsp::DocumentUri>& uris);
    void applyEdit(const lsp::ApplyWorkspaceEditParams& params, const std::optional<ResponseHandler>& handler = std::nullopt);
    void publishDiagnostics(const lsp::PublishDiagnosticsParams& params) override;
    void refreshWorkspaceDiagnostics();
    void terminateWorkspaceDiagnostics(bool retriggerRequest = true);
    void refreshInlayHints();

    void setTrace(const lsp::SetTraceParams& params);

    bool readRawMessage(std::string& output) const;

    void handleResponse(const JsonRpcMessage& message);

private:
    void sendRawMessage(const json& message) const;
};
