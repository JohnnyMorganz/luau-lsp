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

/// Base client interface. LSPClient provides a full LSP implementation;
/// other subclasses (e.g. CliClient) can override only what they need.
/// Virtual methods with empty bodies are no-ops by default.
struct Client
{
    virtual ~Client() = default;

    lsp::ClientCapabilities capabilities;
    /// A registered definitions file passed by the client
    std::unordered_map<std::string, std::string> definitionsFiles{};
    /// A registered documentation file passed by the client
    std::vector<std::string> documentationFiles{};
    /// Parsed documentation database
    Luau::DocumentationDatabase documentation{""};
    /// Global configuration. These are the default settings that we will use if we don't have the workspace stored in configStore
    ClientConfiguration globalConfig{};

    virtual ClientConfiguration getConfiguration(const lsp::DocumentUri& uri) = 0;

    virtual std::optional<lsp::ProgressToken> getWorkspaceDiagnosticsToken() const
    {
        return std::nullopt;
    }

    virtual void sendLogMessage(const lsp::MessageType& type, const std::string& message) const = 0;

    virtual void publishDiagnostics(const lsp::PublishDiagnosticsParams& params) = 0;

    virtual void sendTrace(const std::string& message, const std::optional<std::string>& verbose = std::nullopt) const {}

    virtual void sendWindowMessage(const lsp::MessageType& type, const std::string& message) const {}

    virtual void registerCapability(const std::string& registrationId, const std::string& method, const json& registerOptions) {}

    virtual void unregisterCapability(const std::string& registrationId, const std::string& method) {}

    virtual void sendProgress(const lsp::ProgressParams& params) {}

    virtual void createWorkDoneProgress(const lsp::ProgressToken& token) {}

    virtual void sendWorkDoneProgressBegin(const lsp::ProgressToken& token, const std::string& title,
        std::optional<std::string> message = std::nullopt, std::optional<uint8_t> percentage = std::nullopt)
    {
    }

    virtual void sendWorkDoneProgressReport(
        const lsp::ProgressToken& token, std::optional<std::string> message = std::nullopt, std::optional<uint8_t> percentage = std::nullopt)
    {
    }

    virtual void sendWorkDoneProgressEnd(const lsp::ProgressToken& token, std::optional<std::string> message = std::nullopt) {}

    virtual void refreshWorkspaceDiagnostics() {}

    virtual void terminateWorkspaceDiagnostics(bool retriggerRequest = true) {}

    virtual void refreshInlayHints() {}

    virtual void applyEdit(const lsp::ApplyWorkspaceEditParams& params, const std::optional<ResponseHandler>& handler = std::nullopt) {}

    virtual void sendNotification(const std::string& method, const std::optional<json>& params) const {}
};

class LSPClient : public Client
{
public:
    lsp::TraceValue traceMode = lsp::TraceValue::Off;
    /// Configuration passed from the language client. Currently we only handle configuration at the workspace level
    std::unordered_map<Uri, ClientConfiguration, UriHash> configStore{};

    ConfigChangedCallback configChangedCallback;

    std::optional<lsp::ProgressToken> workspaceDiagnosticsToken = std::nullopt;
    std::optional<id_type> workspaceDiagnosticsRequestId = std::nullopt;

    std::optional<lsp::ProgressToken> getWorkspaceDiagnosticsToken() const override
    {
        return workspaceDiagnosticsToken;
    }

private:
    std::unique_ptr<Transport> transport;
    /// The request id for the next request
    int nextRequestId = 0;
    std::unordered_map<id_type, ResponseHandler> responseHandler{};

public:
    LSPClient();
    LSPClient(std::unique_ptr<Transport> transport);

    virtual void sendRequest(const id_type& id, const std::string& method, const std::optional<json>& params,
        const std::optional<ResponseHandler>& handler = std::nullopt);
    void sendResponse(const id_type& id, const json& result);
    virtual void sendError(const std::optional<id_type>& id, const JsonRpcException& e);
    void sendNotification(const std::string& method, const std::optional<json>& params) const override;

    void sendProgress(const lsp::ProgressParams& params) override
    {
        sendNotification("$/progress", params);
    }
    void createWorkDoneProgress(const lsp::ProgressToken& token) override;
    void sendWorkDoneProgressBegin(const lsp::ProgressToken& token, const std::string& title, std::optional<std::string> message = std::nullopt,
        std::optional<uint8_t> percentage = std::nullopt) override;
    void sendWorkDoneProgressReport(
        const lsp::ProgressToken& token, std::optional<std::string> message = std::nullopt, std::optional<uint8_t> percentage = std::nullopt) override;
    void sendWorkDoneProgressEnd(const lsp::ProgressToken& token, std::optional<std::string> message = std::nullopt) override;

    void sendLogMessage(const lsp::MessageType& type, const std::string& message) const override;
    void sendTrace(const std::string& message, const std::optional<std::string>& verbose = std::nullopt) const override;
    void sendWindowMessage(const lsp::MessageType& type, const std::string& message) const override;

    void registerCapability(const std::string& registrationId, const std::string& method, const json& registerOptions) override;
    void unregisterCapability(const std::string& registrationId, const std::string& method) override;

    ClientConfiguration getConfiguration(const lsp::DocumentUri& uri) override;
    void removeConfiguration(const lsp::DocumentUri& uri);
    // TODO: this function only supports getting requests for workspaces
    void requestConfiguration(const std::vector<lsp::DocumentUri>& uris);
    void applyEdit(const lsp::ApplyWorkspaceEditParams& params, const std::optional<ResponseHandler>& handler = std::nullopt) override;
    void publishDiagnostics(const lsp::PublishDiagnosticsParams& params) override;
    void refreshWorkspaceDiagnostics() override;
    void terminateWorkspaceDiagnostics(bool retriggerRequest = true) override;
    void refreshInlayHints() override;

    void setTrace(const lsp::SetTraceParams& params);

    bool readRawMessage(std::string& output) const;

    void handleResponse(const JsonRpcMessage& message);

private:
    void sendRawMessage(const json& message) const;
};
