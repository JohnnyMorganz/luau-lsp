#pragma once

#include "LSP/Client.hpp"

struct CliClient : public BaseClient
{
    ClientConfiguration configuration;
    mutable std::vector<std::pair<std::filesystem::path, std::string>> diagnostics{};

    const ClientConfiguration getConfiguration(const lsp::DocumentUri& uri) override
    {
        return configuration;
    }

    // In the CLI, this is only used for config errors right now
    void publishDiagnostics(const lsp::PublishDiagnosticsParams& params) override
    {
        for (const auto& diagnostic : params.diagnostics)
            diagnostics.push_back(std::pair{params.uri, diagnostic.message});
    }
};
