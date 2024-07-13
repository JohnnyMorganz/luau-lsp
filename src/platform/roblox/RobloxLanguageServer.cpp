#include <Platform/RobloxPlatform.hpp>

#include <LSP/Workspace.hpp>

void RobloxPlatform::onDidChangeWatchedFiles(const lsp::FileEvent& change)
{
    auto filePath = change.uri.fsPath();
    auto config = workspaceFolder->client->getConfiguration(workspaceFolder->rootUri);
    std::string sourcemapFileName = config.sourcemap.sourcemapFile;

    // Flag sourcemap changes
    if (filePath.filename() == sourcemapFileName)
    {
        workspaceFolder->client->sendLogMessage(lsp::MessageType::Info, "Registering sourcemap changed for workspace " + workspaceFolder->name);
        updateSourceMap();

        // Recompute diagnostics
        workspaceFolder->recomputeDiagnostics(config);
    }
}

void RobloxPlatform::setupWithConfiguration(const ClientConfiguration& config)
{
    std::shared_ptr<Client>& client = workspaceFolder->client;

    if (config.sourcemap.enabled)
    {
        std::string sourcemapFileName = config.sourcemap.sourcemapFile;

        client->sendTrace("workspace: sourcemap enabled");
        if (!workspaceFolder->isNullWorkspace() && !updateSourceMap())
        {
            client->sendWindowMessage(lsp::MessageType::Error,
                "Failed to load " + sourcemapFileName + " for workspace '" + workspaceFolder->name + "'. Instance information will not be available");
        }

        if (client->capabilities.workspace && client->capabilities.workspace->didChangeWatchedFiles &&
            client->capabilities.workspace->didChangeWatchedFiles->dynamicRegistration)
        {
            client->sendLogMessage(lsp::MessageType::Info, "registering didChangedWatchedFiles capability");

            std::vector<lsp::FileSystemWatcher> watchers{};
            watchers.push_back(lsp::FileSystemWatcher{"**/" + sourcemapFileName});
            client->registerCapability(
                "didChangedWatchedFilesCapability", "workspace/didChangeWatchedFiles", lsp::DidChangeWatchedFilesRegistrationOptions{watchers});
        }
        else
        {
            client->unregisterCapability(
                "didChangedWatchedFilesCapability", "workspace/didChangeWatchedFiles");
            client->sendLogMessage(lsp::MessageType::Warning,
                "client does not allow didChangeWatchedFiles registration - automatic updating on sourcemap/config changes disabled");
        }
    }
    else
    {
        client->unregisterCapability(
            "didChangedWatchedFilesCapability", "workspace/didChangeWatchedFiles");
        client->sendLogMessage(lsp::MessageType::Info,
            "sourcemap is disabled - automatic updating on sourcemap/config changes disabled");
    }
}
