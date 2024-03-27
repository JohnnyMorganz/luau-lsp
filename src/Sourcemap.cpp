#include <optional>
#include <filesystem>
#include "LSP/Sourcemap.hpp"
#include "LSP/Utils.hpp"
#include "Luau/StringUtils.h"

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

std::optional<SourceNodePtr> SourceNode::findChild(const std::string& childName)
{
    for (const auto& child : children)
        if (child->name == childName)
            return child;
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
        for (auto& [key, value] : val.items())
        {
            out += "[\"" + key + "\"] = ";
            out += jsonValueToLuau(value);
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

std::string tomlValueToLuau(const tomlValue& val)
{
    if (val.is_string())
    {
        std::string str = val.as_string();
        return '"' + Luau::escape(str) + '"';
    }
    else if (val.is_integer() || val.is_floating() || val.is_boolean())
    {
        return toml::format(val);
    }
    else if (val.is_uninitialized())
    {
        // unreachable
        return "nil";
    }
    else if (val.is_array())
    {
        std::string out = "{";
        for (auto& elem : val.as_array())
        {
            out += tomlValueToLuau(elem);
            out += ";";
        }

        out += "}";
        return out;
    }
    else if (val.is_table())
    {
        std::string out = "{";
        for (auto& [key, value] : val.as_table())
        {
            out += "[\"" + Luau::escape(key) + "\"] = ";
            out += tomlValueToLuau(value);
            out += ";";
        }

        out += "}";
        return out;
    }
    // TODO: support datetime?
    else
    {
        return ""; // TODO: should we error here?
    }
}