#include "Luau/FileResolver.h"
#include "Platform/StringRequireSuggester.hpp"
#include "LSP/Workspace.hpp"

#include <filesystem>


std::string FileRequireNode::getLabel() const
{
    return path.filename().string();
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
    auto basePath = path.parent_path();

    std::filesystem::path relativeNodePath;
    if (auto luaurcAlias = resolveAlias(requireString, mainRequirerNodeConfig, basePath))
        relativeNodePath = luaurcAlias.value();
    else if (path.has_parent_path())
    {
        if (isInitLuauFile(path))
            relativeNodePath = basePath.parent_path().append(requireString);
        else
            relativeNodePath = basePath.append(requireString);
    }
    else
        return nullptr;

    return std::make_unique<FileRequireNode>(relativeNodePath, std::filesystem::is_directory(relativeNodePath), workspaceFolder);
}

std::vector<std::unique_ptr<Luau::RequireNode>> FileRequireNode::getChildren() const
{
    std::vector<std::unique_ptr<Luau::RequireNode>> results;

    if (isDirectory)
    {
        try
        {
            for (const auto& dir_entry : std::filesystem::directory_iterator(path))
            {
                if ((dir_entry.is_regular_file() && !isInitLuauFile(dir_entry.path())) || dir_entry.is_directory())
                {
                    if (workspaceFolder->isIgnoredFileForAutoImports(Uri::file(dir_entry.path())))
                        continue;

                    std::string fileName = dir_entry.path().filename().generic_string();
                    results.emplace_back(std::make_unique<FileRequireNode>(dir_entry.path(), dir_entry.is_directory(), workspaceFolder));
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
    if (isInitLuauFile(path))
        results.emplace_back(Luau::RequireAlias{"self", {"Alias"}});

    return results;
}

std::unique_ptr<Luau::RequireNode> StringRequireSuggester::getNode(const Luau::ModuleName& name) const
{
    if (auto realPath = platform->resolveToRealPath(name))
    {
        auto config = configResolver->getConfig(name);
        return std::make_unique<FileRequireNode>(*realPath, std::filesystem::is_directory(*realPath), workspaceFolder, config);
    }

    return nullptr;
}
