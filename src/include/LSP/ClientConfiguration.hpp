#pragma once
#include <vector>
#include "nlohmann/json.hpp"

struct ClientDiagnosticsConfiguration
{
    /// Whether to also compute diagnostics for dependents when a file changes
    bool includeDependents = true;
    /// Whether to compute diagnostics for a whole workspace
    bool workspace = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientDiagnosticsConfiguration, includeDependents, workspace);

// These are the passed configuration options by the client, prefixed with `luau-lsp.`
// Here we also define the default settings
struct ClientConfiguration
{
    /// Whether to automatically autocomplete end
    bool autocompleteEnd = false;
    std::vector<std::string> ignoreGlobs;
    ClientDiagnosticsConfiguration diagnostics;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientConfiguration, autocompleteEnd, ignoreGlobs, diagnostics);