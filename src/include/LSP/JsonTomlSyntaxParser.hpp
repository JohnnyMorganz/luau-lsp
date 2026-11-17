#pragma once

#include "nlohmann/json.hpp"
#include "toml11/toml.hpp"

#ifndef _RYML_SINGLE_HEADER_AMALGAMATED_HPP_
#include "ryml.hpp"
#endif

#include <string>
#include <string_view>

/// Converts JSONC source to plain JSON by removing `//` and `/* */` comments and trailing commas
std::string jsoncToJson(std::string_view source);
std::string jsonValueToLuau(const nlohmann::json& val);
std::string tomlValueToLuau(const toml::value& val);
std::string yamlValueToLuau(ryml::ConstNodeRef node);