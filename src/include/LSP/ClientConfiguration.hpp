#pragma once
#include <vector>
#include "nlohmann/json.hpp"

// These are the passed configuration options by the client, prefixed with `luau-lsp.`
// Here we also define the default settings
struct ClientConfiguration
{
    /// Whether to automatically autocomplete end
    bool autocompleteEnd = false;
    std::vector<std::string> ignoreGlobs;
    // std::unordered_map<std::string, std::variant<int, bool>> fastFlags; // TODO: add ability to configure fast flags
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientConfiguration, autocompleteEnd, ignoreGlobs);