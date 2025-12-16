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

    /// Debug: print package info to stderr
    static void debugPrint(const RotrieverPackage& package);
};

} // namespace Luau::LanguageServer
