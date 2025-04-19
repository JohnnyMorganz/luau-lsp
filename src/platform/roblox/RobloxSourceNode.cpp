#include "Platform/RobloxPlatform.hpp"
#include <queue>

bool SourceNode::isScript()
{
    return className == "ModuleScript" || className == "Script" || className == "LocalScript";
}

/// NOTE: Use `WorkspaceFileResolver::getRealPathFromSourceNode()` instead of this function where
/// possible, as that will ensure it is relative to the correct workspace root.
std::optional<std::filesystem::path> SourceNode::getScriptFilePath()
{
    for (const auto& path : filePaths)
    {
        if (path.extension() == ".lua" || path.extension() == ".luau")
        {
            return path;
        }
        else if (path.extension() == ".json" && isScript() && !endsWith(path.filename().generic_string(), ".meta.json"))
        {
            return path;
        }
        else if (path.extension() == ".toml" && isScript())
        {
            return path;
        }
    }
    return std::nullopt;
}

Luau::SourceCode::Type SourceNode::sourceCodeType() const
{
    if (className == "ServerScript" or className == "LocalScript")
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

std::optional<SourceNodePtr> SourceNode::findChild(const std::string& childName)
{
    for (const auto& child : children)
        if (child->name == childName)
            return child;
    return std::nullopt;
}

std::optional<SourceNodePtr> SourceNode::findDescendant(const std::string& childName)
{
    // Peforms a BFS search
    std::queue<SourceNodePtr> queue{};

    // TODO: this isn't so great, we shouldn't really be making a new shared ptr here
    // but since we know this will never get returned, we'll leave it alone for now
    queue.push(std::make_shared<SourceNode>(*this));

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

std::optional<SourceNodePtr> SourceNode::findAncestor(const std::string& ancestorName)
{
    auto current = parent;
    while (auto currentPtr = current.lock())
    {
        if (currentPtr->name == ancestorName)
            return currentPtr;
        current = currentPtr->parent;
    }
    return std::nullopt;
}
