#include "doctest.h"
#include "Platform/RotrieverResolver.hpp"
#include "TempDir.h"

#include <fstream>

using namespace Luau::LanguageServer;

TEST_SUITE_BEGIN("RotrieverResolver");

TEST_CASE("parse_simple_rotriever_toml")
{
    TempDir tempDir("rotriever_test");
    
    // Create a simple rotriever.toml
    auto tomlPath = tempDir.path() + "/rotriever.toml";
    {
        std::ofstream file(tomlPath);
        file << R"(
[package]
name = "TestPackage"
version = "1.0.0"
content_root = "src"

[dependencies]
React = { path = "../react" }
Promise = { path = "../promise" }

[dev_dependencies]
JestGlobals = { path = "../jest-globals" }
)";
    }
    
    RotrieverResolver resolver;
    auto result = resolver.parseManifest(Uri::file(tomlPath));
    
    REQUIRE(result.has_value());
    
    auto& package = *result;
    
    CHECK_EQ(package.name, "TestPackage");
    CHECK_EQ(package.version, "1.0.0");
    CHECK_EQ(package.contentRoot, "src");
    
    // Check dependencies
    REQUIRE_EQ(package.dependencies.size(), 2);
    CHECK(package.dependencies.find("React") != package.dependencies.end());
    CHECK(package.dependencies.find("Promise") != package.dependencies.end());
    CHECK_EQ(package.dependencies.at("React").path, "../react");
    CHECK_EQ(package.dependencies.at("Promise").path, "../promise");
    
    // Check dev dependencies
    REQUIRE_EQ(package.devDependencies.size(), 1);
    CHECK(package.devDependencies.find("JestGlobals") != package.devDependencies.end());
    CHECK_EQ(package.devDependencies.at("JestGlobals").path, "../jest-globals");
}

TEST_CASE("parse_rotriever_toml_with_defaults")
{
    TempDir tempDir("rotriever_test");
    
    // Minimal rotriever.toml without content_root
    auto tomlPath = tempDir.path() + "/rotriever.toml";
    {
        std::ofstream file(tomlPath);
        file << R"(
[package]
name = "MinimalPackage"
version = "0.1.0"
)";
    }
    
    RotrieverResolver resolver;
    auto result = resolver.parseManifest(Uri::file(tomlPath));
    
    REQUIRE(result.has_value());
    CHECK_EQ(result->name, "MinimalPackage");
    CHECK_EQ(result->contentRoot, "src"); // Default value
    CHECK_EQ(result->dependencies.size(), 0);
    CHECK_EQ(result->devDependencies.size(), 0);
}

TEST_CASE("parse_nonexistent_file_returns_nullopt")
{
    RotrieverResolver resolver;
    auto result = resolver.parseManifest(Uri::file("/nonexistent/rotriever.toml"));
    
    CHECK_FALSE(result.has_value());
}

TEST_CASE("resolved_paths_are_absolute")
{
    TempDir tempDir("rotriever_test");
    
    auto tomlPath = tempDir.path() + "/rotriever.toml";
    {
        std::ofstream file(tomlPath);
        file << R"(
[package]
name = "TestPackage"
version = "1.0.0"

[dependencies]
SiblingPackage = { path = "../sibling" }
)";
    }
    
    RotrieverResolver resolver;
    auto result = resolver.parseManifest(Uri::file(tomlPath));
    
    REQUIRE(result.has_value());
    REQUIRE(result->dependencies.find("SiblingPackage") != result->dependencies.end());
    
    auto& dep = result->dependencies.at("SiblingPackage");
    
    // The resolved path should be absolute, not relative
    CHECK_FALSE(dep.resolvedPath.fsPath().empty());
    // It should start with /
    CHECK(dep.resolvedPath.fsPath()[0] == '/');
}

TEST_CASE("parse_complex_rotriever_toml_like_lua_apps")
{
    TempDir tempDir("rotriever_test");
    
    // Create a complex rotriever.toml similar to lua-apps
    auto tomlPath = tempDir.path() + "/rotriever.toml";
    {
        std::ofstream file(tomlPath);
        file << R"(
[package]
name = "GameCollections"
version = "0.1.0"
publish = true
authors = ["Personalization <personalization@roblox.com>"]
content_root = "src"
files = ["*"]

[dependencies]
React = { path = "../../proxy-packages/react" }
GameTile = { path = "../game-tile" }
Promise = { path = "../../proxy-packages/promise" }

[dev_dependencies]
JestGlobals = { path = "../../proxy-packages/jest-globals-3" }
)";
    }
    
    RotrieverResolver resolver;
    auto result = resolver.parseManifest(Uri::file(tomlPath));
    
    REQUIRE(result.has_value());
    
    auto& package = *result;
    
    CHECK_EQ(package.name, "GameCollections");
    CHECK_EQ(package.version, "0.1.0");
    CHECK_EQ(package.contentRoot, "src");
    
    // Check all dependencies parsed
    REQUIRE_EQ(package.dependencies.size(), 3);
    CHECK(package.dependencies.find("React") != package.dependencies.end());
    CHECK(package.dependencies.find("GameTile") != package.dependencies.end());
    CHECK(package.dependencies.find("Promise") != package.dependencies.end());
    
    // Check path values
    CHECK_EQ(package.dependencies.at("GameTile").path, "../game-tile");
    CHECK_EQ(package.dependencies.at("React").path, "../../proxy-packages/react");
    
    // Check dev dependencies
    REQUIRE_EQ(package.devDependencies.size(), 1);
    CHECK(package.devDependencies.find("JestGlobals") != package.devDependencies.end());
    
    // Debug print for manual verification
    RotrieverResolver::debugPrint(package);
}

TEST_CASE("parseExports_simple_table")
{
    TempDir tempDir("rotriever_exports_test");
    
    auto initLuaPath = tempDir.path() + "/init.lua";
    {
        std::ofstream file(initLuaPath);
        file << R"(
local Foo = require(script.Foo)
local Bar = require(script.Bar)

return {
    Foo = Foo,
    Bar = Bar,
    SomeConstant = 42,
}
)";
    }
    
    auto exports = RotrieverResolver::parseExports(Uri::file(initLuaPath));
    
    REQUIRE_EQ(exports.size(), 3);
    CHECK(std::find(exports.begin(), exports.end(), "Foo") != exports.end());
    CHECK(std::find(exports.begin(), exports.end(), "Bar") != exports.end());
    CHECK(std::find(exports.begin(), exports.end(), "SomeConstant") != exports.end());
}

TEST_CASE("parseExports_complex_like_game_tile")
{
    TempDir tempDir("rotriever_exports_test");
    
    auto initLuaPath = tempDir.path() + "/init.lua";
    {
        std::ofstream file(initLuaPath);
        file << R"(
local GameTileView = require(script.GameTileView)
local GameTileConstants = require(script.GameTileConstants)
local AppGameTile = require(script.AppGameTile)

return {
    AppGameTile = AppGameTile,
    GameTileConstants = GameTileConstants,
    GameTileView = GameTileView,
}
)";
    }
    
    auto exports = RotrieverResolver::parseExports(Uri::file(initLuaPath));
    
    REQUIRE_EQ(exports.size(), 3);
    CHECK(std::find(exports.begin(), exports.end(), "AppGameTile") != exports.end());
    CHECK(std::find(exports.begin(), exports.end(), "GameTileConstants") != exports.end());
    CHECK(std::find(exports.begin(), exports.end(), "GameTileView") != exports.end());
}

TEST_CASE("parseExports_nonexistent_file")
{
    auto exports = RotrieverResolver::parseExports(Uri::file("/nonexistent/init.lua"));
    CHECK_EQ(exports.size(), 0);
}

TEST_CASE("parseExports_no_return_table")
{
    TempDir tempDir("rotriever_exports_test");
    
    auto initLuaPath = tempDir.path() + "/init.lua";
    {
        std::ofstream file(initLuaPath);
        file << R"(
-- Module that returns a function instead of a table
return function()
    return "hello"
end
)";
    }
    
    auto exports = RotrieverResolver::parseExports(Uri::file(initLuaPath));
    CHECK_EQ(exports.size(), 0);
}

TEST_CASE("parseExports_with_conditional_exports")
{
    TempDir tempDir("rotriever_exports_test");
    
    auto initLuaPath = tempDir.path() + "/init.lua";
    {
        std::ofstream file(initLuaPath);
        // This simulates the real GameTile init.lua pattern with conditional exports
        file << R"(
local Foo = require(script.Foo)
local Bar = require(script.Bar)
local FFlagEnabled = true

return {
    Foo = Foo,
    Bar = Bar,
    ConditionalExport = if FFlagEnabled then require(script.Conditional) else nil,
}
)";
    }
    
    auto exports = RotrieverResolver::parseExports(Uri::file(initLuaPath));
    
    // We should get at least the simple exports
    // Note: The conditional export might not be parsed as a simple Record item
    CHECK(exports.size() >= 2);
    CHECK(std::find(exports.begin(), exports.end(), "Foo") != exports.end());
    CHECK(std::find(exports.begin(), exports.end(), "Bar") != exports.end());
}

TEST_SUITE_END();

