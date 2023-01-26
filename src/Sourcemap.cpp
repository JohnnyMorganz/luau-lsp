#include <optional>
#include <filesystem>
#include "LSP/Sourcemap.hpp"
#include "LSP/Utils.hpp"

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
    }
    return std::nullopt;
}

Luau::SourceCode::Type SourceNode::sourceCodeType() const
{
    if (className == "ServerScript")
    {
        return Luau::SourceCode::Type::Script;
    }
    else if (className == "LocalScript")
    {
        return Luau::SourceCode::Type::Local;
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

std::optional<SourceNodePtr> SourceNode::findChild(const std::string& name)
{
    for (const auto& child : children)
    {
        if (child->name == name)
        {
            return child;
        }
    }
    return std::nullopt;
}

std::optional<SourceNodePtr> SourceNode::findAncestor(const std::string& name)
{
    auto current = parent;
    while (auto currentPtr = current.lock())
    {
        if (currentPtr->name == name)
            return currentPtr;
        current = currentPtr->parent;
    }
    return std::nullopt;
}

Luau::SourceCode::Type sourceCodeTypeFromPath(const std::filesystem::path& requirePath)
{
    auto filename = requirePath.filename().generic_string();
    if (endsWith(filename, ".server.lua") || endsWith(filename, ".server.luau"))
    {
        return Luau::SourceCode::Type::Script;
    }
    else if (endsWith(filename, ".client.lua") || endsWith(filename, ".client.luau"))
    {
        return Luau::SourceCode::Type::Local;
    }

    return Luau::SourceCode::Type::Module;
}

std::string jsonValueToLuau(const json& val)
{
    if (val.is_string() || val.is_number() || val.is_boolean())
    {
        return val.dump();
    }
    else if (val.is_null())
    {
        return "nil";
    }
    else if (val.is_array())
    {
        std::string out = "{";
        for (auto& elem : val)
        {
            out += jsonValueToLuau(elem);
            out += ";";
        }

        out += "}";
        return out;
    }
    else if (val.is_object())
    {
        std::string out = "{";

        for (auto& [key, val] : val.items())
        {
            out += "[\"" + key + "\"] = ";
            out += jsonValueToLuau(val);
            out += ";";
        }

        out += "}";
        return out;
    }
    else
    {
        return ""; // TODO: should we error here?
    }
}