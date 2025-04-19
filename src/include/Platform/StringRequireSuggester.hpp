#pragma once

#include "LSP/WorkspaceFileResolver.hpp"
#include <filesystem>

class FileRequireNode : public Luau::RequireNode
{
public:
    FileRequireNode(std::filesystem::path path, bool isDirectory)
        : path(std::move(path))
        , isDirectory(isDirectory)
    {
    }

    FileRequireNode(std::filesystem::path path, bool isDirectory, Luau::Config mainRequirerNodeConfig)
        : path(std::move(path))
        , isDirectory(isDirectory)
        , mainRequirerNodeConfig(std::move(mainRequirerNodeConfig))
    {
    }

    std::string getPathComponent() const override;
    std::string getLabel() const override;
    std::vector<std::string> getTags() const override;
    std::unique_ptr<Luau::RequireNode> resolvePathToNode(const std::string& path) const override;
    std::vector<std::unique_ptr<Luau::RequireNode>> getChildren() const override;
    std::vector<Luau::RequireAlias> getAvailableAliases() const override;

private:
    std::filesystem::path path;
    bool isDirectory = false;

    /// The resolved configuration for the main requirer node
    /// This is for alias resolution
    Luau::Config mainRequirerNodeConfig;
};

class StringRequireSuggester : public Luau::RequireSuggester
{
public:
    StringRequireSuggester(Luau::ConfigResolver* configResolver, LSPPlatform* platform)
        : configResolver(configResolver)
        , platform(platform)
    {
    }

protected:
    std::unique_ptr<Luau::RequireNode> getNode(const Luau::ModuleName& name) const override;

private:
    Luau::ConfigResolver* configResolver;
    LSPPlatform* platform;
};