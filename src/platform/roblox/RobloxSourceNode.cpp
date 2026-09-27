#include "LSP/JsonRpc.hpp"
#include "Platform/RobloxPlatform.hpp"
#include <queue>

SourceNode::SourceNode(
    std::string name, std::string className, std::vector<std::string> filePaths, std::vector<SourceNode*> children, bool pluginManaged)
    : name(std::move(name))
    , className(std::move(className))
    , filePaths(std::move(filePaths))
    , children(std::move(children))
    , pluginManaged(pluginManaged)
{
}

bool SourceNode::isScript() const
{
    return className == "ModuleScript" || className == "Script" || className == "LocalScript";
}

/// NOTE: Use `WorkspaceFileResolver::getRealPathFromSourceNode()` instead of this function where
/// possible, as that will ensure it is relative to the correct workspace root.
std::optional<std::string> SourceNode::getScriptFilePath() const
{
    for (const auto& path : filePaths)
    {
        const auto uri = Uri::file(path);
        if (uri.extension() == ".lua" || uri.extension() == ".luau")
        {
            return path;
        }
        else if (uri.extension() == ".json" && isScript() && !endsWith(uri.filename(), ".meta.json"))
        {
            return path;
        }
        else if (uri.extension() == ".toml" && isScript())
        {
            return path;
        }
        else if ((uri.extension() == ".yaml" || uri.extension() == ".yml") && isScript())
        {
            return path;
        }
    }
    return std::nullopt;
}

Luau::SourceCode::Type SourceNode::sourceCodeType() const
{
    if (className == "ServerScript" || className == "LocalScript")
    {
        return Luau::SourceCode::Type::Script;
    }
    else if (className == "ModuleScript")
    {
        return Luau::SourceCode::Type::Module;
    }
    else
    {
        return Luau::SourceCode::Type::None;
    }
}

std::optional<SourceNode*> SourceNode::findChild(const std::string& childName) const
{
    for (const auto& child : children)
        if (child->name == childName)
            return child;
    return std::nullopt;
}

std::optional<const SourceNode*> SourceNode::findDescendant(const std::string& childName) const
{
    // Peforms a BFS search
    std::queue<const SourceNode*> queue{};

    // TODO: this isn't so great, we shouldn't really be making a new shared ptr here
    // but since we know this will never get returned, we'll leave it alone for now
    queue.push(this);

    while (!queue.empty())
    {
        auto next = queue.front();
        queue.pop();

        for (const auto& child : next->children)
        {
            if (child->name == childName)
                return child;
            queue.push(child);
        }
    }
    return std::nullopt;
}

bool SourceNode::containsFilePaths() const
{
    // Explicit-stack post-order traversal to avoid stack overflow on deeply nested sourcemaps
    // (see #1521). Each frame accumulates whether any child visited so far had file paths, so the
    // aggregate for a node is available as soon as its last child has been popped.
    struct Frame
    {
        const SourceNode* node;
        size_t nextChildIndex = 0;
        bool anyChildHasFilePaths = false;
    };

    std::vector<Frame> stack;
    stack.push_back(Frame{this});

    bool result = false;

    while (!stack.empty())
    {
        const SourceNode* node = stack.back().node;

        if (stack.back().nextChildIndex < node->children.size())
        {
            const SourceNode* child = node->children[stack.back().nextChildIndex];
            stack.back().nextChildIndex++;
            stack.push_back(Frame{child});
            continue;
        }

        result = !node->filePaths.empty() || stack.back().anyChildHasFilePaths;
        stack.pop_back();
        if (!stack.empty())
            stack.back().anyChildHasFilePaths = stack.back().anyChildHasFilePaths || result;
    }

    return result;
}

std::optional<const SourceNode*> SourceNode::findAncestor(const std::string& ancestorName) const
{
    auto current = parent;
    while (current)
    {
        if (current->name == ancestorName)
            return current;
        current = current->parent;
    }
    return std::nullopt;
}

bool SourceNode::isAncestorOf(const SourceNode* other) const
{
    for (auto n = other->parent; n; n = n->parent)
        if (n == this)
            return true;
    return false;
}

const SourceNode* SourceNode::walkPath(const std::string& path) const
{
    const SourceNode* base = this;
    size_t start = 0;
    while (start < path.size())
    {
        if (path.compare(start, 2, "./") == 0)
        {
            start += 2;
            continue;
        }

        size_t end = path.find('/', start);
        std::string segment = (end == std::string::npos) ? path.substr(start) : path.substr(start, end - start);
        start = (end == std::string::npos) ? path.size() : end + 1;

        if (segment.empty() || segment == ".")
            continue;

        if (segment == "..")
        {
            base = base->parent;
            if (!base)
                return nullptr;
        }
        else
        {
            auto child = base->findChild(segment);
            if (!child)
                return nullptr;
            base = *child;
        }
    }

    return base;
}

void SourceNode::clearCachedTypes() const
{
    // Explicit-stack traversal: a deeply nested sourcemap (e.g. from `rojo sourcemap
    // --include-non-scripts` mirroring an unpacked model's instance tree) can be too deep to
    // walk recursively without overflowing the stack. See #1521.
    std::vector<const SourceNode*> stack{this};
    while (!stack.empty())
    {
        const SourceNode* node = stack.back();
        stack.pop_back();

        node->tys.clear();
        for (const auto& child : node->children)
            stack.push_back(child);
    }
}

SourceNode* SourceNode::fromJson(const json& j, Luau::TypedAllocator<SourceNode>& allocator)
{
    // Explicit-stack post-order build: a deeply nested sourcemap (e.g. from `rojo sourcemap
    // --include-non-scripts` mirroring an unpacked model's instance tree) can be too deep to
    // parse recursively without overflowing the stack. See #1521. Each frame tracks the json
    // node it corresponds to, how many of its "children" have been visited so far, and the
    // SourceNode children already built for it; a node is only constructed once every entry in
    // its "children" array has been turned into a SourceNode.
    struct Frame
    {
        const json* value;
        size_t nextChildIndex = 0;
        std::vector<SourceNode*> children;
    };

    std::vector<Frame> stack;
    stack.push_back(Frame{&j});

    SourceNode* result = nullptr;

    while (!stack.empty())
    {
        const json& node = *stack.back().value;
        const json* childrenArray = node.contains("children") ? &node.at("children") : nullptr;

        if (childrenArray && stack.back().nextChildIndex < childrenArray->size())
        {
            const json& childJson = childrenArray->at(stack.back().nextChildIndex);
            stack.back().nextChildIndex++;
            stack.push_back(Frame{&childJson});
            continue;
        }

        auto name = node.at("name").get<std::string>();
        auto className = node.at("className").get<std::string>();

        std::vector<std::string> filePaths;
        if (node.contains("filePaths"))
            node.at("filePaths").get_to(filePaths);

        bool pluginManaged = node.contains("pluginManaged") && node.at("pluginManaged").get<bool>();

        result = allocator.allocate(
            SourceNode(std::move(name), std::move(className), std::move(filePaths), std::move(stack.back().children), pluginManaged));

        stack.pop_back();
        if (!stack.empty())
            stack.back().children.push_back(result);
    }

    return result;
}

// Only includes nodes with filepaths to avoid writing every Instance in the DataModel to `sourcemap.json`
ordered_json SourceNode::toJson() const
{
    // Explicit-stack post-order build, mirroring `fromJson`, to avoid stack overflow on a deeply
    // nested in-memory tree (see #1521). Whether a node's subtree "contains file paths" is
    // computed inline as part of this same walk (each frame tracks whether any child visited so
    // far had file paths, same as `containsFilePaths()`), rather than by calling
    // `containsFilePaths()` on each candidate child before descending into it - doing so would
    // make the whole walk quadratic, since that call would re-walk the child's subtree from
    // scratch at every level. A child is only attached to its parent's "children" array (and, for
    // consistency, only allowed to influence the parent's own aggregate) once it's known its
    // subtree has something worth keeping; `this` is always kept regardless, matching the
    // recursive version never pruning the node `toJson()` was called on.
    struct Frame
    {
        const SourceNode* node;
        size_t nextChildIndex = 0;
        ordered_json children = ordered_json::array();
        bool anyChildHasFilePaths = false;
    };

    std::vector<Frame> stack;
    stack.push_back(Frame{this});

    ordered_json result;

    while (!stack.empty())
    {
        const SourceNode* node = stack.back().node;

        if (stack.back().nextChildIndex < node->children.size())
        {
            const SourceNode* child = node->children[stack.back().nextChildIndex];
            stack.back().nextChildIndex++;
            stack.push_back(Frame{child});
            continue;
        }

        bool hasFilePaths = !node->filePaths.empty() || stack.back().anyChildHasFilePaths;
        bool isRoot = stack.size() == 1;

        if (hasFilePaths || isRoot)
        {
            ordered_json nodeJson;
            nodeJson["name"] = node->name;
            nodeJson["className"] = node->className;
            if (node->pluginManaged)
            {
                // When a plugin-managed node is no longer in the plugin info, it must be pruned.
                // However, when the sourcemap is re-read (ex: file change, reopened editor, LSP restart)
                // that would make all nodes NOT plugin-managed, so nothing could ever be removed after that.
                // Therefore, we need to persist pluginManaged in the json.
                nodeJson["pluginManaged"] = node->pluginManaged;
            }

            if (!node->filePaths.empty())
                nodeJson["filePaths"] = node->filePaths;

            if (!stack.back().children.empty())
                nodeJson["children"] = std::move(stack.back().children);

            result = std::move(nodeJson);
        }

        stack.pop_back();
        if (!stack.empty() && hasFilePaths)
        {
            stack.back().anyChildHasFilePaths = true;
            stack.back().children.push_back(std::move(result));
        }
    }

    return result;
}
