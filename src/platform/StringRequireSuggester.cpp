#include "Luau/FileResolver.h"
#include "Platform/StringRequireSuggester.hpp"
#include "LSP/Workspace.hpp"

#include <filesystem>


std::string FileRequireNode::getLabel() const
{
    return uri.filename();
}

std::string FileRequireNode::getPathComponent() const
{
    auto name = getLabel();
    if (auto pos = name.find_last_of('.'); pos != std::string::npos)
        name.erase(pos);
    return name;
}

std::vector<std::string> FileRequireNode::getTags() const
{
    if (isDirectory)
        return {"Directory"};
    else
        return {"File"};
}

std::unique_ptr<Luau::RequireNode> FileRequireNode::resolvePathToNode(const std::string& requireString) const
{
    auto basePath = uri.parent();
    if (!basePath)
        return nullptr;

    Uri relativeNodeUri;
    if (auto luaurcAlias = resolveAlias(requireString, mainRequirerNodeConfig, *basePath))
        relativeNodeUri = luaurcAlias.value();
    else if (isInitLuauFile(uri.fsPath()))
    {
        if (auto parent = basePath->parent())
            relativeNodeUri = parent->resolvePath(requireString);
        else
            return nullptr;
    }
    else
        relativeNodeUri = basePath->resolvePath(requireString);

    return std::make_unique<FileRequireNode>(relativeNodeUri, relativeNodeUri.isDirectory(), workspaceFolder);
}

std::vector<std::unique_ptr<Luau::RequireNode>> FileRequireNode::getChildren() const
{
    std::vector<std::unique_ptr<Luau::RequireNode>> results;

    if (isDirectory)
    {
        try
        {
            for (const auto& dir_entry : std::filesystem::directory_iterator(uri.fsPath()))
            {
                if ((dir_entry.is_regular_file() && !isInitLuauFile(dir_entry.path())) || dir_entry.is_directory())
                {
                    if (workspaceFolder->isIgnoredFileForAutoImports(Uri::file(dir_entry.path())))
                        continue;

                    std::string fileName = dir_entry.path().filename().generic_string();
                    results.emplace_back(std::make_unique<FileRequireNode>(Uri::file(dir_entry.path()), dir_entry.is_directory(), workspaceFolder));
                }
            }
        }
        catch (std::exception&)
        {
            // TODO: error?
        }
    }

    return results;
}

std::vector<Luau::RequireAlias> FileRequireNode::getAvailableAliases() const
{
    std::vector<Luau::RequireAlias> results;

    for (const auto& [_, aliasInfo] : mainRequirerNodeConfig.aliases)
        results.emplace_back(Luau::RequireAlias{aliasInfo.originalCase, {"Alias"}});

    // Include @self alias for init.lua files
    if (isInitLuauFile(uri.fsPath()))
        results.emplace_back(Luau::RequireAlias{"self", {"Alias"}});

    return results;
}

std::unique_ptr<Luau::RequireNode> StringRequireSuggester::getNode(const Luau::ModuleName& name) const
{
    if (auto realUri = platform->resolveToRealPath(name))
    {
        auto config = configResolver->getConfig(name);
        return std::make_unique<FileRequireNode>(*realUri, realUri->isDirectory(), workspaceFolder, config);
    }

    return nullptr;
}
