#include "Platform/RobloxPlatform.hpp"

#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"

void RobloxPlatform::onStudioPluginFullChange(const PluginNode& dataModel)
{
    workspaceFolder->client->sendLogMessage(lsp::MessageType::Info, "received full change from studio plugin");

    // TODO: properly handle multi-workspace setup
    pluginInfo = std::make_shared<PluginNode>(dataModel);

    // Mutate the sourcemap with the new information
    updateSourceMap();
}

void RobloxPlatform::onStudioPluginClear()
{
    workspaceFolder->client->sendLogMessage(lsp::MessageType::Info, "received clear from studio plugin");

    // TODO: properly handle multi-workspace setup
    pluginInfo = nullptr;

    // Mutate the sourcemap with the new information
    updateSourceMap();
}

bool RobloxPlatform::handleNotification(const std::string& method, std::optional<json> params)
{
    if (method == "$/plugin/full")
    {
        onStudioPluginFullChange(JSON_REQUIRED_PARAMS(params, "$/plugin/full"));
    }
    else if (method == "$/plugin/clear")
    {
        onStudioPluginClear();
    }

    return false;
}
