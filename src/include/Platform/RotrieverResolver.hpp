#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

#include "LSP/Uri.hpp"
#include "Luau/Ast.h"

namespace Luau::LanguageServer
{

/// A single dependency from rotriever.toml
struct RotrieverDependency
{
    std::string name; // e.g., "GameTile"
    std::string path; // e.g., "../game-tile"
    Uri resolvedPath; // Absolute path to the dependency
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
};

/// Parses rotriever.toml files and package exports
class RotrieverResolver
{
public:
    /// Parse a rotriever.toml file
    /// Returns the parsed package, or nullopt on failure
    std::optional<RotrieverPackage> parseManifest(const Uri& manifestPath);

    /// Parse the init.lua file to extract exported names (reads and parses the file)
    /// Returns the list of exported names (keys in the return table)
    static std::vector<std::string> parseExports(const Uri& initLuaPath);

    /// Extract exports from an already-parsed AST root
    /// This is useful when the module is already indexed by the Frontend
    static std::vector<std::string> extractExportsFromAst(Luau::AstStatBlock* root);

    /// Debug: print package info to stderr
    static void debugPrint(const RotrieverPackage& package);
};

} // namespace Luau::LanguageServer
