#pragma once

#include "LSP/WorkspaceFileResolver.hpp"
#include <filesystem>

class FileRequireNode : public Luau::RequireNode
{
public:
    FileRequireNode(Uri uri, bool isDirectory, WorkspaceFolder* workspaceFolder)
        : uri(std::move(uri))
        , isDirectory(isDirectory)
        , workspaceFolder(workspaceFolder)
    {
    }

    FileRequireNode(Uri uri, bool isDirectory, WorkspaceFolder* workspaceFolder, Luau::Config mainRequirerNodeConfig)
        : uri(uri)
        , isDirectory(isDirectory)
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
    Uri uri;
    bool isDirectory = false;

    /// The resolved configuration for the main requirer node
    /// This is for alias resolution
    Luau::Config mainRequirerNodeConfig;

    /// The workspace folder that the files belong to
    /// Used to check ignoreGlobs
    WorkspaceFolder* workspaceFolder;
};

class StringRequireSuggester : public Luau::RequireSuggester
{
public:
    StringRequireSuggester(WorkspaceFolder* workspaceFolder, Luau::ConfigResolver* configResolver, LSPPlatform* platform)
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
    LSPPlatform* platform;
};