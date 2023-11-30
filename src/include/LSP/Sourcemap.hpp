#pragma once
#include <optional>
#include <filesystem>
#include <Luau/FileResolver.h>
#include "Luau/Type.h"
#include "Luau/TypeInfer.h"
#include "Luau/GlobalTypes.h"
#include "nlohmann/json.hpp"
#include "LSP/PluginDataModel.hpp"
using json = nlohmann::json;

using SourceNodePtr = std::shared_ptr<struct SourceNode>;

struct SourceNode
{
    std::weak_ptr<struct SourceNode> parent; // Can be null! NOT POPULATED BY SOURCEMAP, must be written to manually
    std::string name;
    std::string className;
    std::vector<std::filesystem::path> filePaths{};
    std::vector<SourceNodePtr> children{};
    std::string virtualPath; // NB: NOT POPULATED BY SOURCEMAP, must be written to manually
    // The corresponding TypeId for this sourcemap node
    // A different TypeId is created for each type checker (frontend.typeChecker and frontend.typeCheckerForAutocomplete)
    std::unordered_map<Luau::GlobalTypes const*, Luau::TypeId> tys{}; // NB: NOT POPULATED BY SOURCEMAP, created manually. Can be null!

    bool isScript();
    std::optional<std::filesystem::path> getScriptFilePath();
    Luau::SourceCode::Type sourceCodeType() const;
    std::optional<SourceNodePtr> findChild(const std::string& name);
    // O(n) search for ancestor of name
    std::optional<SourceNodePtr> findAncestor(const std::string& name);
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
