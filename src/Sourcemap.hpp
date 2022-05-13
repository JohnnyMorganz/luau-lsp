#pragma once
#include <optional>
#include <filesystem>
#include <Luau/FileResolver.h>
#include "nlohmann/json.hpp"
using json = nlohmann::json;

using SourceNodePtr = std::shared_ptr<struct SourceNode>;

struct SourceNode
{
    std::string name;
    std::string className;
    std::vector<std::filesystem::path> filePaths;
    std::vector<SourceNodePtr> children;
    std::string virtualPath; // NB: NOT POPULATED BY SOURCEMAP, must be written to manually

    bool isScript()
    {
        return className == "ModuleScript" || className == "Script" || className == "LocalScript";
    }

    std::optional<std::filesystem::path> getScriptFilePath()
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

    Luau::SourceCode::Type sourceCodeType() const
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
};

void from_json(const json& j, SourceNode& p)
{
    j.at("name").get_to(p.name);
    j.at("className").get_to(p.className);

    if (j.contains("filePaths"))
        j.at("filePaths").get_to(p.filePaths);

    if (j.contains("children"))
    {
        for (auto& child : j.at("children"))
        {
            p.children.push_back(std::make_shared<SourceNode>(child.get<SourceNode>()));
        }
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