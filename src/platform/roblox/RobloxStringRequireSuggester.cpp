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
        // Derive the real file URI for alias resolution (needed for @self on init.luau files)
        if (auto filePath = node->getScriptFilePath())
        {
            if (auto nodeUri = workspaceFolder->fileResolver.rootUri.resolvePath(*filePath).parent())
            {
                if (auto aliasedPath = resolveAlias(requireString, *mainRequirerNodeConfig, *nodeUri))
                    return std::make_unique<FileRequireNode>(*aliasedPath, aliasedPath->isDirectory(), workspaceFolder);
            }
        }

        if (auto alias = parseAliasRequire(requireString); alias && alias->name == kGameAlias && rootNode)
        {
            if (auto targetNode = rootNode->walkPath(alias->remainder))
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
    if (!mainRequirerNodeConfig->aliases.find(kGameAlias))
        results.emplace_back(Luau::RequireAlias{kGameAlias, {"Alias"}});

    if (auto filePath = node->getScriptFilePath())
    {
        // Include @self alias for init.luau files
        if (isInitLuauFile(workspaceFolder->fileResolver.rootUri.resolvePath(*filePath)))
            results.emplace_back(Luau::RequireAlias{"self", {"Alias"}});
    }

    return results;
}

std::unique_ptr<Luau::RequireNode> SourcemapAwareFileRequireNode::resolvePathToNode(const std::string& requireString) const
{
    LUAU_ASSERT(mainRequirerNodeConfig);

    // `@game` is rooted at the DataModel, so it resolves against the sourcemap root even though this
    // file has no node. A user-defined `game` alias in `.luaurc` still wins, and every other form
    // keeps the filesystem behaviour.
    if (auto alias = parseAliasRequire(requireString);
        alias && alias->name == kGameAlias && rootNode && !mainRequirerNodeConfig->aliases.find(kGameAlias))
    {
        if (auto targetNode = rootNode->walkPath(alias->remainder))
            return std::make_unique<SourceNodeRequireNode>(targetNode, rootNode, mainRequirerNodeConfig, workspaceFolder);
        return nullptr;
    }

    return FileRequireNode::resolvePathToNode(requireString);
}

std::vector<Luau::RequireAlias> SourcemapAwareFileRequireNode::getAvailableAliases() const
{
    LUAU_ASSERT(mainRequirerNodeConfig);

    auto results = FileRequireNode::getAvailableAliases();

    if (rootNode && !mainRequirerNodeConfig->aliases.find(kGameAlias))
        results.emplace_back(Luau::RequireAlias{kGameAlias, {"Alias"}});

    return results;
}

std::unique_ptr<Luau::RequireNode> RobloxStringRequireSuggester::getNode(const Luau::ModuleName& name) const
{
    auto config = std::make_shared<const Luau::Config>(configResolver->getConfig(name, workspaceFolder->limits));

    if (auto it = platform->virtualPathsToSourceNodes.find(name); it != platform->virtualPathsToSourceNodes.end())
        return std::make_unique<SourceNodeRequireNode>(it->second, platform->rootSourceNode, std::move(config), workspaceFolder);

    // Fall back to filesystem-based node for modules not in the sourcemap. It still offers `@game`,
    // which does not depend on the requirer having a node.
    if (auto realUri = platform->resolveToRealPath(name))
        return std::make_unique<SourcemapAwareFileRequireNode>(
            *realUri, realUri->isDirectory(), platform->rootSourceNode, workspaceFolder, std::move(config));

    return nullptr;
}
