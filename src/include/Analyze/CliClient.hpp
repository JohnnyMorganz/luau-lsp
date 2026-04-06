#pragma once

#include "LSP/Client.hpp"

inline std::string getMessageTypeString(const lsp::MessageType& type)
{
    switch (type)
    {
    case lsp::MessageType::Error:
        return "ERROR";
    case lsp::MessageType::Warning:
        return "WARN";
    case lsp::MessageType::Info:
        return "INFO";
    case lsp::MessageType::Log:
        return "LOG";
    }
    return "LOG";
}

struct CliClient : public Client
{
    mutable std::vector<std::pair<Uri, std::string>> diagnostics{};

    ClientConfiguration getConfiguration(const lsp::DocumentUri& uri) override
    {
        return globalConfig;
    }

    void sendLogMessage(const lsp::MessageType& type, const std::string& message) const override
    {
        std::cerr << "[" << getMessageTypeString(type) << "] " << message << "\n";
    }

    void sendWindowMessage(const lsp::MessageType& type, const std::string& message) const override
    {
        sendLogMessage(type, message);
    }

    // In the CLI, this is only used for config errors right now
    void publishDiagnostics(const lsp::PublishDiagnosticsParams& params) override
    {
        for (const auto& diagnostic : params.diagnostics)
            diagnostics.emplace_back(params.uri, diagnostic.message);
    }
};
