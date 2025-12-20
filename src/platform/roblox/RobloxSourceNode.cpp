#include "Platform/RobloxPlatform.hpp"
#include <queue>

SourceNode::SourceNode(std::string name, std::string className, std::vector<std::string> filePaths, std::vector<SourceNode*> children)
    : name(std::move(name))
    , className(std::move(className))
    , filePaths(std::move(filePaths))
    , children(std::move(children))
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

SourceNode* SourceNode::fromJson(const json& j, Luau::TypedAllocator<SourceNode>& allocator)
{
    auto name = j.at("name").get<std::string>();
    auto className = j.at("className").get<std::string>();

    std::vector<std::string> filePaths;
    if (j.contains("filePaths"))
        j.at("filePaths").get_to(filePaths);

    std::vector<SourceNode*> children;
    if (j.contains("children"))
    {
        for (auto& child : j.at("children"))
            children.emplace_back(SourceNode::fromJson(child, allocator));
    }

    return allocator.allocate(SourceNode(std::move(name), std::move(className), std::move(filePaths), std::move(children)));
}