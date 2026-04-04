#pragma once

#include "Luau/Config.h"
#include "Luau/ConfigResolver.h"
#include "Luau/FileResolver.h"

class WorkspaceFolder;
struct SourceNode;
class RobloxPlatform;

class SourceNodeRequireNode : public Luau::RequireNode
{
public:
    SourceNodeRequireNode(const SourceNode* node, const SourceNode* rootNode, Luau::Config mainRequirerNodeConfig, WorkspaceFolder* workspaceFolder)
        : node(node)
        , rootNode(rootNode)
        , mainRequirerNodeConfig(std::move(mainRequirerNodeConfig))
        , workspaceFolder(workspaceFolder)
    {
    }

    std::string getPathComponent() const override;
    std::string getLabel() const override;
    std::vector<std::string> getTags() const override;
    std::unique_ptr<Luau::RequireNode> resolvePathToNode(const std::string& path) const override;
    std::vector<std::unique_ptr<Luau::RequireNode>> getChildren() const override;
    std::vector<Luau::RequireAlias> getAvailableAliases() const override;

private:
    const SourceNode* node;
    const SourceNode* rootNode;
    Luau::Config mainRequirerNodeConfig;
    WorkspaceFolder* workspaceFolder;
};

class RobloxStringRequireSuggester : public Luau::RequireSuggester
{
public:
    RobloxStringRequireSuggester(
        WorkspaceFolder* workspaceFolder, Luau::ConfigResolver* configResolver, const RobloxPlatform* platform)
        : workspaceFolder(workspaceFolder)
        , configResolver(configResolver)
        , platform(platform)
    {
    }

protected:
    std::unique_ptr<Luau::RequireNode> getNode(const Luau::ModuleName& name) const override;

private:
    WorkspaceFolder* workspaceFolder;
    Luau::ConfigResolver* configResolver;
    const RobloxPlatform* platform;
};
