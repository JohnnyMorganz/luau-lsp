#pragma once
#include <optional>
#include <filesystem>
#include "nlohmann/json.hpp"
using json = nlohmann::json;

using PluginNodePtr = std::shared_ptr<struct PluginNode>;

struct PluginNode
{
    std::string name = "";
    std::string className = "";
    std::vector<PluginNodePtr> children{};
};

static void from_json(const json& j, PluginNode& p)
{
    j.at("Name").get_to(p.name);
    j.at("ClassName").get_to(p.className);

    if (j.contains("Children"))
    {
        for (auto& child : j.at("Children"))
        {
            p.children.push_back(std::make_shared<PluginNode>(child.get<PluginNode>()));
        }
    }
}