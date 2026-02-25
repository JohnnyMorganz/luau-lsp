#pragma once

#include "nlohmann/json.hpp"
#include "toml11/toml.hpp"

#ifndef _RYML_SINGLE_HEADER_AMALGAMATED_HPP_
#include "ryml.hpp"
#endif

#include <string>

std::string jsonValueToLuau(const nlohmann::json& val, bool literalStrings = false);
std::string tomlValueToLuau(const toml::value& val, bool literalStrings = false);
std::string yamlValueToLuau(ryml::ConstNodeRef node, bool literalStrings = false);