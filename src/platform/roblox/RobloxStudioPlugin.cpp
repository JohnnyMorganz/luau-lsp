#include "LuauFileUtils.hpp"
#include "Platform/RobloxPlatform.hpp"

#include "LSP/Utils.hpp"
#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"

static bool mutateSourceNodeWithPluginInfo(SourceNode* sourceNode, const PluginNode* pluginInstance, Luau::TypedAllocator<SourceNode>& allocator)
{
    bool didUpdateSourcemap = false;

    // Update paths if managed or syncing
    if ((sourceNode->pluginManaged || !pluginInstance->filePaths.empty()) && sourceNode->filePaths != pluginInstance->filePaths)
    {
        didUpdateSourcemap = true;
        sourceNode->filePaths = pluginInstance->filePaths;
    }
    // Update class if managed (eg: renaming foo.client.lua to foo.server.luau needs the foo node to change class)
    if (sourceNode->pluginManaged && sourceNode->className != pluginInstance->className)
    {
        didUpdateSourcemap = true;
        sourceNode->className = pluginInstance->className;
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
            // The pruned subtree stays allocated but becomes unreachable from the root, so
            // clearSourcemapTypes can no longer clear its cached types. Clear them now so a
            // retained reference to the node cannot resurrect types from a destroyed arena
            child->clearCachedTypes();
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

static std::vector<std::string> pluginFilePathsFromJson(const json& j)
{
    std::vector<std::string> filePaths{};
    for (auto& filePath : j)
    {
        filePaths.emplace_back(Luau::FileUtils::normalizePath(resolvePath(filePath.get<std::string>())));
    }
    return filePaths;
}

PluginNode* PluginNode::fromJson(const json& j, Luau::TypedAllocator<PluginNode>& allocator)
{
    auto name = j.at("Name").get<std::string>();
    auto className = j.at("ClassName").get<std::string>();
    std::vector<std::string> filePaths{};
    if (j.contains("FilePaths"))
        filePaths = pluginFilePathsFromJson(j.at("FilePaths"));

    std::vector<PluginNode*> children;
    if (j.contains("Children"))
    {
        for (auto& child : j.at("Children"))
        {
            children.emplace_back(PluginNode::fromJson(child, allocator));
        }
    }

    auto node = allocator.allocate(PluginNode{std::move(name), std::move(className), std::move(filePaths), std::move(children)});
    if (j.contains("Id"))
        node->id = j.at("Id").get<uint64_t>();
    for (auto* child : node->children)
        child->parent = node;
    return node;
}

static PluginNode* clonePluginNode(const PluginNode* node, PluginNode* parent, Luau::TypedAllocator<PluginNode>& allocator)
{
    auto clone = allocator.allocate(PluginNode{node->name, node->className, node->filePaths, {}, node->id, parent});
    clone->children.reserve(node->children.size());
    for (const auto* child : node->children)
        clone->children.push_back(clonePluginNode(child, clone, allocator));
    return clone;
}

static void detachPluginNode(PluginNode* node)
{
    if (!node->parent)
        return;
    auto& siblings = node->parent->children;
    if (auto it = std::find(siblings.begin(), siblings.end(), node); it != siblings.end())
        siblings.erase(it);
    node->parent = nullptr;
}

void RobloxPlatform::indexPluginNodes(PluginNode* node)
{
    if (node->id == 0)
        throw std::runtime_error("instance '" + node->name + "' is missing an Id");
    if (!pluginNodesById.emplace(node->id, node).second)
        throw std::runtime_error("duplicate instance Id " + std::to_string(node->id));
    for (auto* child : node->children)
        indexPluginNodes(child);
}

void RobloxPlatform::unindexPluginNodes(const PluginNode* node)
{
    pluginNodesById.erase(node->id);
    for (const auto* child : node->children)
        unindexPluginNodes(child);
}

PluginNode* RobloxPlatform::getPluginNode(const json& id) const
{
    auto it = pluginNodesById.find(id.get<uint64_t>());
    if (it == pluginNodesById.end())
        throw std::runtime_error("unknown instance Id " + id.dump());
    return it->second;
}

void RobloxPlatform::applyPluginChange(const json& change)
{
    auto type = change.at("Type").get<std::string>();
    if (type == "Add")
    {
        auto parent = getPluginNode(change.at("Parent"));
        auto node = PluginNode::fromJson(change.at("Instance"), pluginNodeAllocator);
        indexPluginNodes(node);
        node->parent = parent;
        parent->children.push_back(node);
    }
    else if (type == "Remove")
    {
        auto node = getPluginNode(change.at("Id"));
        if (node == pluginInfo)
            throw std::runtime_error("cannot remove the root instance");
        detachPluginNode(node);
        unindexPluginNodes(node);
    }
    else if (type == "Update")
    {
        auto node = getPluginNode(change.at("Id"));
        if (change.contains("Name"))
            node->name = change.at("Name").get<std::string>();
        if (change.contains("FilePaths"))
            node->filePaths = pluginFilePathsFromJson(change.at("FilePaths"));
        if (change.contains("Parent"))
        {
            auto newParent = getPluginNode(change.at("Parent"));
            if (node == pluginInfo)
                throw std::runtime_error("cannot reparent the root instance");
            for (auto ancestor = newParent; ancestor; ancestor = ancestor->parent)
                if (ancestor == node)
                    throw std::runtime_error("cannot parent an instance to its own descendant");
            detachPluginNode(node);
            node->parent = newParent;
            newParent->children.push_back(node);
        }
    }
    else
    {
        throw std::runtime_error("unknown change type '" + type + "'");
    }
}

void RobloxPlatform::compactPluginNodes()
{
    // Removed nodes stay allocated until the allocator is cleared. Once most of the allocator is garbage,
    // copy the live tree into a fresh allocator so that long editing sessions do not grow memory unbounded
    if (!pluginInfo || pluginNodeAllocator.size() < 4096 || pluginNodeAllocator.size() < 2 * pluginNodesById.size())
        return;

    Luau::TypedAllocator<PluginNode> compacted;
    auto root = clonePluginNode(pluginInfo, nullptr, compacted);
    pluginNodeAllocator.clear();
    pluginNodeAllocator = std::move(compacted);
    pluginInfo = root;

    pluginNodesById.clear();
    indexPluginNodes(pluginInfo);
}

void RobloxPlatform::clearPluginManagedNodesFromSourcemap(SourceNode* sourceNode)
{
    for (auto it = sourceNode->children.begin(); it != sourceNode->children.end();)
    {
        auto* child = *it;

        if (child->pluginManaged)
        {
            child->clearCachedTypes();
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

    bool didUpdateSourcemap = mutateSourceNodeWithPluginInfo(rootSourceNode, pluginInfo, sourceNodeAllocator);

    if (didUpdateSourcemap)
    {
        // Update the sourcemap file if needed
        auto config = workspaceFolder->client->getConfiguration(workspaceFolder->rootUri);
        if (config.sourcemap.enabled && config.sourcemap.autogenerate)
        {
            auto sourcemapPath = workspaceFolder->rootUri.resolvePath(config.sourcemap.sourcemapFile);

            workspaceFolder->client->sendLogMessage(
                lsp::MessageType::Info, "Updating " + config.sourcemap.sourcemapFile + " with information from plugin");

            try
            {
                Luau::FileUtils::writeFileIfModified(sourcemapPath.fsPath(), rootSourceNode->toJson().dump(2));
            }
            catch (const std::exception& e)
            {
                std::cerr << "Failed to write sourcemap file: " << e.what() << '\n';
            }
        }
    }

    return didUpdateSourcemap;
}

void RobloxPlatform::onPluginInfoChanged()
{
    hydrateSourcemapWithPluginInfo();

    // Rebuild the path maps from scratch: hydration may have pruned nodes, and writePathsToMap
    // only overwrites entries for nodes still in the tree. A stale entry pointing at a pruned
    // node would let later type checks reach its (freed) cached sourcemap types
    rebuildPathMaps();
    updateSourcemapTypes();
}

void RobloxPlatform::onStudioPluginFullChange(const json& dataModel)
{
    workspaceFolder->client->sendLogMessage(lsp::MessageType::Info, "received full change from studio plugin");

    // Parse into a new allocator, so that the current plugin info stays valid if the DataModel is malformed
    Luau::TypedAllocator<PluginNode> allocator;
    auto info = PluginNode::fromJson(dataModel, allocator);
    pluginNodeAllocator.clear();
    pluginNodeAllocator = std::move(allocator);
    setPluginInfo(info);

    // Plugins supporting incremental changes identify the full DataModel with a session and give every instance an Id
    if (dataModel.contains("Session"))
    {
        try
        {
            indexPluginNodes(pluginInfo);
            pluginSession = dataModel.at("Session").get<std::string>();
        }
        catch (const std::exception& e)
        {
            pluginNodesById.clear();
            workspaceFolder->client->sendLogMessage(
                lsp::MessageType::Warning, std::string("studio plugin DataModel cannot receive incremental changes: ") + e.what());
        }
    }

    onPluginInfoChanged();
}

bool RobloxPlatform::onStudioPluginChanges(const json& params)
{
    auto session = params.find("session");
    if (!pluginInfo || !pluginSession || session == params.end() || !session->is_string() || session->get<std::string>() != *pluginSession)
    {
        workspaceFolder->client->sendLogMessage(
            lsp::MessageType::Info, "received changes from studio plugin for an unknown session, requesting full DataModel");
        return false;
    }

    try
    {
        for (const auto& change : params.at("changes"))
            applyPluginChange(change);
    }
    catch (const std::exception& e)
    {
        // The plugin tree may have been partially updated, so only accept a new full DataModel from now on
        pluginSession = std::nullopt;
        pluginNodesById.clear();
        workspaceFolder->client->sendLogMessage(
            lsp::MessageType::Warning, std::string("failed to apply changes from studio plugin, requesting full DataModel: ") + e.what());
        return false;
    }

    compactPluginNodes();
    onPluginInfoChanged();
    return true;
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
        rebuildPathMaps();
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
    if (method == "$/plugin/changes")
    {
        if (!onStudioPluginChanges(JSON_REQUIRED_PARAMS(params, "$/plugin/changes")))
            throw JsonRpcException(lsp::ErrorCode::RequestFailed, "studio plugin DataModel is out of sync, a full DataModel is required");
        return json(nullptr);
    }

    return std::nullopt;
}
