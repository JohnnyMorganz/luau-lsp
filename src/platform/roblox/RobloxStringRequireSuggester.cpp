#include "Luau/Common.h"
#include "Platform/RobloxStringRequireSuggester.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "Platform/StringRequireSuggester.hpp"
#include "LSP/Utils.hpp"
#include "LSP/Workspace.hpp"

std::string SourceNodeRequireNode::getPathComponent() const
{
    return node->name;
}

std::string SourceNodeRequireNode::getLabel() const
{
    return node->name;
}

std::vector<std::string> SourceNodeRequireNode::getTags() const
{
    if (node->isScript())
        return {"File"};
    else
        return {"Directory"};
}

std::unique_ptr<Luau::RequireNode> SourceNodeRequireNode::resolvePathToNode(const std::string& requireString) const
{
    LUAU_ASSERT(mainRequirerNodeConfig);

    if (!requireString.empty() && requireString[0] == '@')
    {
        // Check user-defined aliases — fall back to filesystem-based FileRequireNode
        if (auto aliasedPath = resolveAlias(requireString, *mainRequirerNodeConfig, Uri{}))
            return std::make_unique<FileRequireNode>(*aliasedPath, aliasedPath->isDirectory(), workspaceFolder);

        size_t slashPos = requireString.find('/');
        std::string aliasName = requireString.substr(1, slashPos == std::string::npos ? std::string::npos : slashPos - 1);
        std::string aliasNameLower = toLower(aliasName);

        if (aliasNameLower == "game" && rootNode)
        {
            std::string remainder = (slashPos == std::string::npos) ? "" : requireString.substr(slashPos + 1);
            auto targetNode = rootNode->walkPath(remainder);
            if (targetNode)
                return std::make_unique<SourceNodeRequireNode>(targetNode, rootNode, mainRequirerNodeConfig, workspaceFolder);
        }

        return nullptr;
    }

    const SourceNode* baseNode = node->parent;
    if (!baseNode)
        return nullptr;

    auto targetNode = baseNode->walkPath(requireString);
    if (targetNode)
        return std::make_unique<SourceNodeRequireNode>(targetNode, rootNode, mainRequirerNodeConfig, workspaceFolder);

    return nullptr;
}

std::vector<std::unique_ptr<Luau::RequireNode>> SourceNodeRequireNode::getChildren() const
{
    std::vector<std::unique_ptr<Luau::RequireNode>> results;

    for (const auto& child : node->children)
    {
        if (child->isScript() || !child->children.empty())
            results.emplace_back(std::make_unique<SourceNodeRequireNode>(child, rootNode, mainRequirerNodeConfig, workspaceFolder));
    }

    return results;
}

std::vector<Luau::RequireAlias> SourceNodeRequireNode::getAvailableAliases() const
{
    LUAU_ASSERT(mainRequirerNodeConfig);

    std::vector<Luau::RequireAlias> results;

    for (const auto& [_, aliasInfo] : mainRequirerNodeConfig->aliases)
        results.emplace_back(Luau::RequireAlias{aliasInfo.originalCase, {"Alias"}});

    // Add built-in @game alias if not user-defined
    std::string gameLower = "game";
    if (!mainRequirerNodeConfig->aliases.find(gameLower))
        results.emplace_back(Luau::RequireAlias{"game", {"Alias"}});

    return results;
}

std::unique_ptr<Luau::RequireNode> RobloxStringRequireSuggester::getNode(const Luau::ModuleName& name) const
{
    auto config = std::make_shared<const Luau::Config>(configResolver->getConfig(name, workspaceFolder->limits));

    if (auto it = platform->virtualPathsToSourceNodes.find(name); it != platform->virtualPathsToSourceNodes.end())
        return std::make_unique<SourceNodeRequireNode>(it->second, platform->rootSourceNode, std::move(config), workspaceFolder);

    // Fall back to filesystem-based node for modules not in the sourcemap
    if (auto realUri = platform->resolveToRealPath(name))
        return std::make_unique<FileRequireNode>(*realUri, realUri->isDirectory(), workspaceFolder, std::move(config));

    return nullptr;
}
