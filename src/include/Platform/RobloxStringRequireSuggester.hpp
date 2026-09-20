#pragma once

#include "Platform/StringRequireSuggester.hpp"

#include "Luau/Config.h"
#include "Luau/ConfigResolver.h"
#include "Luau/FileResolver.h"

#include <memory>

class WorkspaceFolder;
struct SourceNode;
class RobloxPlatform;

class SourceNodeRequireNode : public Luau::RequireNode
{
public:
    SourceNodeRequireNode(
        const SourceNode* node, const SourceNode* rootNode, std::shared_ptr<const Luau::Config> mainRequirerNodeConfig, WorkspaceFolder* workspaceFolder)
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
    std::shared_ptr<const Luau::Config> mainRequirerNodeConfig;
    WorkspaceFolder* workspaceFolder;
};

/// A requirer that the sourcemap does not cover, such as build output. It keeps its filesystem
/// identity for relative requires and `.luaurc` aliases, and adds the built-in `@game` alias, which
/// is absolute and so needs the sourcemap root rather than the requirer.
class SourcemapAwareFileRequireNode : public FileRequireNode
{
public:
    SourcemapAwareFileRequireNode(Uri uri, bool isDirectory, const SourceNode* rootNode, WorkspaceFolder* workspaceFolder,
        std::shared_ptr<const Luau::Config> mainRequirerNodeConfig)
        : FileRequireNode(std::move(uri), isDirectory, workspaceFolder, std::move(mainRequirerNodeConfig))
        , rootNode(rootNode)
    {
    }

    std::unique_ptr<Luau::RequireNode> resolvePathToNode(const std::string& path) const override;
    std::vector<Luau::RequireAlias> getAvailableAliases() const override;

private:
    const SourceNode* rootNode;
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
