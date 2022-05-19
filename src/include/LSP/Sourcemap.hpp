#pragma once
#include <optional>
#include <filesystem>
#include <Luau/FileResolver.h>
#include "nlohmann/json.hpp"
using json = nlohmann::json;

using SourceNodePtr = std::shared_ptr<struct SourceNode>;

struct SourceNode
{
    std::weak_ptr<struct SourceNode> parent; // Can be null! NOT POPULATED BY SOURCEMAP, must be written to manually
    std::string name;
    std::string className;
    std::vector<std::filesystem::path> filePaths;
    std::vector<SourceNodePtr> children;
    std::string virtualPath; // NB: NOT POPULATED BY SOURCEMAP, must be written to manually

    bool isScript();
    std::optional<std::filesystem::path> getScriptFilePath();
    Luau::SourceCode::Type sourceCodeType() const;
};

static void from_json(const json& j, SourceNode& p)
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
Luau::SourceCode::Type sourceCodeTypeFromPath(const std::filesystem::path& requirePath);
std::string jsonValueToLuau(const json& val);