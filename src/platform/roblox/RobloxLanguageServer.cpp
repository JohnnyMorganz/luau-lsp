#include <Platform/RobloxPlatform.hpp>

#include <LSP/Workspace.hpp>

void RobloxPlatform::onDidChangeWatchedFiles(const lsp::FileEvent& change)
{
    auto filePath = change.uri.fsPath();
    if (filePath.filename() == "sourcemap.json")
    {
        workspaceFolder->client->sendLogMessage(lsp::MessageType::Info, "Registering sourcemap changed for workspace " + workspaceFolder->name);
        updateSourceMap();
    }
}

void RobloxPlatform::setupWithConfiguration(const ClientConfiguration& config)
{
    if (config.sourcemap.enabled)
    {
        workspaceFolder->client->sendTrace("workspace: sourcemap enabled");
        if (!workspaceFolder->isNullWorkspace() && !updateSourceMap())
        {
            workspaceFolder->client->sendWindowMessage(lsp::MessageType::Error,
                "Failed to load sourcemap.json for workspace '" + workspaceFolder->name + "'. Instance information will not be available");
        }
    }
}
