#pragma once

#include "nlohmann/json.hpp"
#include "toml11/toml.hpp"

#ifndef _RYML_SINGLE_HEADER_AMALGAMATED_HPP_
#include "ryml.hpp"
#endif

#include <functional>
#include <string>

struct DataFileToLuauOptions
{
    std::function<bool(const std::string&)> shouldUseStringSingleton;
};

std::string jsonValueToLuau(const nlohmann::json& val, const DataFileToLuauOptions& options = {});
std::string tomlValueToLuau(const toml::value& val, const DataFileToLuauOptions& options = {});
std::string yamlValueToLuau(ryml::ConstNodeRef node, const DataFileToLuauOptions& options = {});
