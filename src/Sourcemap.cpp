#include <optional>
#include <filesystem>
#include "LSP/Sourcemap.hpp"

bool SourceNode::isScript()
{
    return className == "ModuleScript" || className == "Script" || className == "LocalScript";
}

std::optional<std::filesystem::path> SourceNode::getScriptFilePath()
{
    for (const auto& path : filePaths)
    {
        if (path.extension() == ".lua" || path.extension() == ".luau")
        {
            return path;
        }
        else if (path.extension() == ".json" && isScript())
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

static bool endsWith(std::string str, std::string suffix)
{
    return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
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