#include "Platform/LSPPlatform.hpp"

#include "LSP/ClientConfiguration.hpp"
#include "Platform/RobloxPlatform.hpp"

#include <memory>

LSPPlatform::LSPPlatform(WorkspaceFolder* workspaceFolder)
    : workspaceFolder(workspaceFolder)
{
}

std::unique_ptr<LSPPlatform> LSPPlatform::getPlatform(const ClientConfiguration& config, WorkspaceFolder* workspaceFolder)
{
    if (config.types.roblox && config.platform.platform == LSPPlatformConfig::Roblox)
        return std::make_unique<RobloxPlatform>(workspaceFolder);

    return std::make_unique<LSPPlatform>(workspaceFolder);
}
