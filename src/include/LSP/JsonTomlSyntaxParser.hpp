#pragma once

#include "nlohmann/json.hpp"
#include "toml11/toml.hpp"

#include <string>

std::string jsonValueToLuau(const nlohmann::json& val);
std::string tomlValueToLuau(const toml::value& val);