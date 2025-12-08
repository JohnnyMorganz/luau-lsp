#include "LuauFileUtils.hpp"
#include "Platform/RobloxPlatform.hpp"

#include "LSP/Utils.hpp"
#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"

static bool mutateSourceNodeWithPluginInfo(SourceNode* sourceNode, const PluginNode* pluginInstance, Luau::TypedAllocator<SourceNode>& allocator)
{
    bool didUpdateSourcemap = false;

    bool shouldUpdateFilePaths = sourceNode->pluginManaged || !pluginInstance->filePaths.empty();
    if (shouldUpdateFilePaths && sourceNode->filePaths != pluginInstance->filePaths)
    {
        didUpdateSourcemap = true;
        sourceNode->filePaths = pluginInstance->filePaths;
    }

    std::unordered_set<std::string> pluginChildNames;
    for (const auto& dmChild : pluginInstance->children)
    {
        pluginChildNames.insert(dmChild->name);

        if (auto existingChildNode = sourceNode->findChild(dmChild->name))
        {
            // Hydrate the existing child with the plugin info
            didUpdateSourcemap |= mutateSourceNodeWithPluginInfo(*existingChildNode, dmChild, allocator);
        }
        else
        {
            // Create a new child for this plugin node
            auto childNode = allocator.allocate(SourceNode(dmChild->name, dmChild->className, {}, {}));
            childNode->pluginManaged = true;
            mutateSourceNodeWithPluginInfo(childNode, dmChild, allocator);

            sourceNode->children.push_back(childNode);
            didUpdateSourcemap = true;
        }
    }

    // Prune plugin-managed children that no longer exist in the plugin info
    for (auto it = sourceNode->children.begin(); it != sourceNode->children.end();)
    {
        auto* child = *it;

        if (child->pluginManaged && pluginChildNames.find(child->name) == pluginChildNames.end())
        {
            it = sourceNode->children.erase(it);
            didUpdateSourcemap = true;
        }
        else
        {
            ++it;
        }
    }

    return didUpdateSourcemap;
}

PluginNode* PluginNode::fromJson(const json& j, Luau::TypedAllocator<PluginNode>& allocator)
{
    auto name = j.at("Name").get<std::string>();
    auto className = j.at("ClassName").get<std::string>();
    std::vector<std::string> filePaths{};
    if (j.contains("FilePaths"))
    {
        for (auto& filePath : j.at("FilePaths"))
        {
            filePaths.emplace_back(Luau::FileUtils::normalizePath(resolvePath(filePath.get<std::string>())));
        }
    }

    std::vector<PluginNode*> children;
    if (j.contains("Children"))
    {
        for (auto& child : j.at("Children"))
        {
            children.emplace_back(PluginNode::fromJson(child, allocator));
        }
    }

    return allocator.allocate(PluginNode{std::move(name), std::move(className), std::move(filePaths), std::move(children)});
}

void RobloxPlatform::clearPluginManagedNodesFromSourcemap(SourceNode* sourceNode)
{
    for (auto it = sourceNode->children.begin(); it != sourceNode->children.end();)
    {
        auto* child = *it;

        if (child->pluginManaged)
        {
            it = sourceNode->children.erase(it);
        }
        else
        {
            clearPluginManagedNodesFromSourcemap(child);
            ++it;
        }
    }
}

bool RobloxPlatform::hydrateSourcemapWithPluginInfo()
{
    if (!pluginInfo)
    {
        return false;
    }

    // If we don't have a sourcemap yet, we create a DataModel root node
    if (!rootSourceNode)
    {
        rootSourceNode = sourceNodeAllocator.allocate(SourceNode("game", "DataModel", {}, {}));
    }


    if (rootSourceNode->className != "DataModel")
    {
        std::cerr << "Attempted to update plugin information for a non-DM instance" << '\n';
        return false;
    }

    try
    {
        bool didUpdateSourcemap = mutateSourceNodeWithPluginInfo(rootSourceNode, pluginInfo, sourceNodeAllocator);

        if (didUpdateSourcemap)
        {
            // Update the sourcemap file if needed
            auto config = workspaceFolder->client->getConfiguration(workspaceFolder->rootUri);
            if (config.sourcemap.autogenerate)
            {
                auto sourcemapPath = workspaceFolder->rootUri.resolvePath(config.sourcemap.sourcemapFile);

                workspaceFolder->client->sendLogMessage(
                    lsp::MessageType::Info, "Updating " + config.sourcemap.sourcemapFile + " with information from plugin");

                Luau::FileUtils::writeFile(sourcemapPath.fsPath(), rootSourceNode->toJson().dump(2));
            }
        }

        return didUpdateSourcemap;
    }
    catch (const std::exception& e)
    {
        // TODO: log message? NOTE: this function can be called from CLI
        std::cerr << "Updating sourcemap from plugin info failed:" << e.what() << '\n';
    }

    return false;
}

void RobloxPlatform::onStudioPluginFullChange(const json& dataModel)
{
    workspaceFolder->client->sendLogMessage(lsp::MessageType::Info, "received full change from studio plugin");

    pluginNodeAllocator.clear();
    setPluginInfo(PluginNode::fromJson(dataModel, pluginNodeAllocator));

    hydrateSourcemapWithPluginInfo();
    writePathsToMap(rootSourceNode, rootSourceNode->className == "DataModel" ? "game" : "ProjectRoot");
    updateSourcemapTypes();
}

void RobloxPlatform::onStudioPluginClear()
{
    workspaceFolder->client->sendLogMessage(lsp::MessageType::Info, "received clear from studio plugin");

    // TODO: properly handle multi-workspace setup
    pluginNodeAllocator.clear();
    setPluginInfo(nullptr);

    if (rootSourceNode)
    {
        clearPluginManagedNodesFromSourcemap(rootSourceNode);
        writePathsToMap(rootSourceNode, rootSourceNode->className == "DataModel" ? "game" : "ProjectRoot");
        updateSourcemapTypes();
    }
}

bool RobloxPlatform::handleNotification(const std::string& method, std::optional<json> params)
{
    if (method == "$/plugin/full")
    {
        onStudioPluginFullChange(JSON_REQUIRED_PARAMS(params, "$/plugin/full"));
        return true;
    }
    else if (method == "$/plugin/clear")
    {
        onStudioPluginClear();
        return true;
    }

    return false;
}

std::optional<json> RobloxPlatform::handleRequest(const std::string& method, std::optional<json> params)
{
    if (method == "$/plugin/getFilePaths")
    {
        // Custom request to get all Luau file paths in the workspace for plugin communication
        json result;
        std::vector<std::string> allFiles;

        // Recursively traverse the workspace directory to find all .lua and .luau files
        std::string workspacePath = workspaceFolder->rootUri.fsPath();

        Luau::FileUtils::traverseDirectoryRecursive(workspacePath,
            [&](const std::string& path)
            {
                auto uri = Uri::file(path);
                auto ext = uri.extension();
                if (ext == ".lua" || ext == ".luau")
                {
                    allFiles.push_back(Luau::FileUtils::normalizePath(resolvePath(path)));
                }
            });

        result["files"] = allFiles;
        return result;
    }

    return std::nullopt;
}
