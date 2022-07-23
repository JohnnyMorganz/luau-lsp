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

struct ClientSourcemapConfiguration
{
    /// Whether Rojo sourcemap-related features are enabled
    bool enabled = true;
    /// Whether we should autogenerate the Rojo sourcemap by calling `rojo sourcemap`
    bool autogenerate = true;
    /// The project file to generate a sourcemap for
    std::string rojoProjectFile = "default.project.json";
    /// Whether non script instances should be included in the generated sourcemap
    bool includeNonScripts = true;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientSourcemapConfiguration, enabled, autogenerate, rojoProjectFile, includeNonScripts);

struct ClientTypesConfiguration
{
    /// Whether Roblox-related definitions should be supported
    bool roblox = true;
    /// [DEPRECATED] Any definition files to load globally
    std::vector<std::filesystem::path> definitionFiles;
    /// A map between globs and the environment they are registered too
    std::unordered_map<std::string, std::string> environmentGlobs;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientTypesConfiguration, roblox, definitionFiles, environmentGlobs);

// These are the passed configuration options by the client, prefixed with `luau-lsp.`
// Here we also define the default settings
struct ClientConfiguration
{
    /// Whether to automatically autocomplete end
    bool autocompleteEnd = false;
    std::vector<std::string> ignoreGlobs;
    ClientSourcemapConfiguration sourcemap;
    ClientDiagnosticsConfiguration diagnostics;
    ClientTypesConfiguration types;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientConfiguration, autocompleteEnd, ignoreGlobs, sourcemap, diagnostics, types);