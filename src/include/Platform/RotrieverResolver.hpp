#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

#include "LSP/Uri.hpp"

namespace Luau::LanguageServer
{

/// A single dependency from rotriever.toml
struct RotrieverDependency
{
    std::string name; // e.g., "GameTile"
    std::string path; // e.g., "../game-tile"
    Uri resolvedPath; // Absolute path to the dependency
};

/// Parsed exports from a package's init.lua
struct RotrieverExports
{
    /// Value exports from the return table (e.g., {"GameTileView", "Events.gamePlayIntent"})
    std::vector<std::string> values;
    /// Type exports from "export type" statements (e.g., {"GameTileProps", "GameInfoModel"})
    std::vector<std::string> types;
};

/// An internal module within a Rotriever package
struct RotrieverInternalModule
{
    std::string name;         // e.g., "Button", "helpers"
    std::string relativePath; // Path relative to contentRoot, e.g., "Components/Button", "Utils/helpers"
    Uri absolutePath;         // Full path to the module file

    /// Value exports from the module's return table
    std::vector<std::string> exports;
    /// Type exports from "export type" statements
    std::vector<std::string> typeExports;
};

/// A parsed rotriever.toml package
struct RotrieverPackage
{
    std::string name;        // e.g., "GameCollections"
    std::string version;     // e.g., "0.1.0"
    std::string contentRoot; // e.g., "src"
    Uri packageRoot;         // Directory containing rotriever.toml

    std::unordered_map<std::string, RotrieverDependency> dependencies;
    std::unordered_map<std::string, RotrieverDependency> devDependencies;

    /// Names exported from this package's init.lua
    /// e.g., {"GameTileView", "GameTileConstants", ...}
    std::vector<std::string> exports;

    /// Type names exported via "export type" statements
    /// e.g., {"GameTileProps", "GameInfoModel", ...}
    std::vector<std::string> typeExports;

    /// Internal modules within this package (discovered from contentRoot)
    /// Used for auto-importing from within the same package
    std::vector<RotrieverInternalModule> internalModules;
};

/// Parses rotriever.toml files and package exports
class RotrieverResolver
{
public:
    /// Parse a rotriever.toml file
    /// Returns the parsed package, or nullopt on failure
    std::optional<RotrieverPackage> parseManifest(const Uri& manifestPath);

    /// Parse the init.lua file to extract exported names and types
    /// Returns both value exports (from return table) and type exports (from export type)
    static RotrieverExports parseExports(const Uri& initLuaPath);

    /// Discover all internal modules within a package's content root
    /// Returns a list of modules that can be required from within the package
    static std::vector<RotrieverInternalModule> discoverInternalModules(const Uri& contentRoot);

    /// Compute the script-relative require path from one module to another
    /// e.g., from "Components/Button" to "Utils/helpers" -> "script.Parent.Parent.Utils.helpers"
    static std::string computeScriptRelativePath(const std::string& fromRelativePath, const std::string& toRelativePath);

    /// Debug: print package info to stderr
    static void debugPrint(const RotrieverPackage& package);
};

} // namespace Luau::LanguageServer
