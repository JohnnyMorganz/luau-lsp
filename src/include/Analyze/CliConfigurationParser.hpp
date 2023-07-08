#pragma once

#include <string>
#include <nlohmann/json.hpp>

#include "LSP/ClientConfiguration.hpp"

using json = nlohmann::json;

json parseDottedConfiguration(const std::string& contents);
ClientConfiguration dottedToClientConfiguration(const std::string& contents);