#include "doctest.h"
#include "Fixture.h"
#include "LSP/WorkspaceFileResolver.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "Luau/Ast.h"
#include "Luau/FileResolver.h"
#include "TempDir.h"

TEST_SUITE_BEGIN("WorkspaceFileResolverTests");

TEST_CASE("resolveModule handles LocalPlayer PlayerScripts")
{
    WorkspaceFileResolver fileResolver;
    RobloxPlatform platform{&fileResolver};
    fileResolver.platform = &platform;

    Luau::ModuleInfo baseContext{"game/Players/LocalPlayer/PlayerScripts"};
    auto expr = Luau::AstExprIndexName(Luau::Location(), nullptr, Luau::AstName("PurchaseClient"), Luau::Location(), Luau::Position(0, 0), '.');
    auto resolved = fileResolver.resolveModule(&baseContext, &expr);

    REQUIRE(resolved.has_value());
    CHECK_EQ(resolved->name, "game/StarterPlayer/StarterPlayerScripts/PurchaseClient");
}

TEST_CASE("resolveModule handles LocalPlayer PlayerGui")
{
    WorkspaceFileResolver fileResolver;
    RobloxPlatform platform{&fileResolver};
    fileResolver.platform = &platform;

    Luau::ModuleInfo baseContext{"game/Players/LocalPlayer/PlayerGui"};
    auto expr = Luau::AstExprIndexName(Luau::Location(), nullptr, Luau::AstName("GuiScript"), Luau::Location(), Luau::Position(0, 0), '.');
    auto resolved = fileResolver.resolveModule(&baseContext, &expr);

    REQUIRE(resolved.has_value());
    CHECK_EQ(resolved->name, "game/StarterGui/GuiScript");
}

TEST_CASE("resolveModule handles LocalPlayer StarterGear")
{
    WorkspaceFileResolver fileResolver;
    RobloxPlatform platform{&fileResolver};
    fileResolver.platform = &platform;

    Luau::ModuleInfo baseContext{"game/Players/LocalPlayer/StarterGear"};
    auto expr = Luau::AstExprIndexName(Luau::Location(), nullptr, Luau::AstName("GearScript"), Luau::Location(), Luau::Position(0, 0), '.');
    auto resolved = fileResolver.resolveModule(&baseContext, &expr);

    REQUIRE(resolved.has_value());
    CHECK_EQ(resolved->name, "game/StarterPack/GearScript");
}

TEST_CASE_FIXTURE(Fixture, "resolveModule handles FindFirstChild")
{
    WorkspaceFileResolver fileResolver;
    RobloxPlatform platform{&fileResolver};
    fileResolver.platform = &platform;

    Luau::ModuleInfo baseContext{"game/ReplicatedStorage"};

    Luau::AstStatBlock* block = parse(R"(
        local _ = node:FindFirstChild("Testing")
    )");
    REQUIRE(block != nullptr);
    REQUIRE(block->body.size > 0);

    Luau::AstStatLocal* local = block->body.data[0]->as<Luau::AstStatLocal>();
    REQUIRE(local != nullptr);
    REQUIRE_EQ(1, local->values.size);

    auto resolved = fileResolver.resolveModule(&baseContext, local->values.data[0]);

    REQUIRE(resolved.has_value());
    CHECK_EQ(resolved->name, "game/ReplicatedStorage/Testing");
}

TEST_CASE_FIXTURE(Fixture, "resolveModule fails on FindFirstChild with recursive enabled")
{
    WorkspaceFileResolver fileResolver;
    RobloxPlatform platform{&fileResolver};
    fileResolver.platform = &platform;

    Luau::ModuleInfo baseContext{"game/ReplicatedStorage"};

    Luau::AstStatBlock* block = parse(R"(
        local _ = node:FindFirstChild("Testing", true)
    )");
    REQUIRE(block != nullptr);
    REQUIRE(block->body.size > 0);

    Luau::AstStatLocal* local = block->body.data[0]->as<Luau::AstStatLocal>();
    REQUIRE(local != nullptr);
    REQUIRE_EQ(1, local->values.size);

    auto resolved = fileResolver.resolveModule(&baseContext, local->values.data[0]);

    CHECK_FALSE(resolved.has_value());
}

TEST_CASE_FIXTURE(Fixture, "resolveModule handles FindFirstAncestor")
{
    SourceNode sourceNode;
    sourceNode.name = "Foo";

    WorkspaceFileResolver fileResolver;
    RobloxPlatform platform{&fileResolver};
    fileResolver.platform = &platform;

    platform.rootSourceNode = std::make_shared<SourceNode>(sourceNode);

    Luau::ModuleInfo baseContext{"ProjectRoot/Bar"};

    Luau::AstStatBlock* block = parse(R"(
        local t = node:FindFirstAncestor("Foo")
    )");
    REQUIRE(block != nullptr);
    REQUIRE(block->body.size > 0);

    Luau::AstStatLocal* local = block->body.data[0]->as<Luau::AstStatLocal>();
    REQUIRE(local != nullptr);
    REQUIRE_EQ(1, local->values.size);

    auto resolved = fileResolver.resolveModule(&baseContext, local->values.data[0]);

    REQUIRE(resolved.has_value());
    CHECK_EQ(resolved->name, "ProjectRoot");
}

TEST_CASE_FIXTURE(Fixture, "resolveDirectoryAliases")
{
    std::unordered_map<std::string, std::string> directoryAliases{
        {"@test1/", "C:/Users/test/test1"},
        {"@test2/", "~/test2"},
        {"@relative/", "src/client"},
        {"@test4", "C:/Users/test/test1"},
    };

    auto home = getHomeDirectory();
    REQUIRE(home);

    CHECK_EQ(resolveDirectoryAlias("", directoryAliases, "@test1/foo"), "C:/Users/test/test1/foo");
    CHECK_EQ(resolveDirectoryAlias("", directoryAliases, "@test1/foo.luau"), "C:/Users/test/test1/foo.luau");
    CHECK_EQ(resolveDirectoryAlias("", directoryAliases, "@test1/"), "C:/Users/test/test1");
    // NOTE: we do not strip `/` from `@test1`, so we use it as `@test4`
    // for now we don't "fix" this, because our startsWith check is greedy, so we want to allow differentiation between `@foo/` and `@foobar/`
    CHECK_EQ(resolveDirectoryAlias("", directoryAliases, "@test4"), "C:/Users/test/test1");

    CHECK_EQ(resolveDirectoryAlias("", directoryAliases, "@test2/bar"), home.value() / "test2" / "bar");

    CHECK_EQ(resolveDirectoryAlias("", directoryAliases, "@test3/bar"), std::nullopt);

    // Relative directory aliases
    CHECK_EQ(resolveDirectoryAlias("workspace/", directoryAliases, "@relative/foo.luau"), "workspace/src/client/foo.luau");
}

TEST_CASE_FIXTURE(Fixture, "resolve_alias_does_nothing_if_string_doesnt_start_with_@_symbol")
{
    loadLuaurc(R"(
    {
        "aliases": {
            "test": "test.lua"
        }
    }
    )");

    CHECK_EQ(resolveAlias("test", workspace.fileResolver.defaultConfig), std::nullopt);
}

TEST_CASE_FIXTURE(Fixture, "resolve_alias_handles_variations_with_directory_separator")
{
    loadLuaurc(R"(
    {
        "aliases": {
            "test": "folder"
        }
    }
    )");

    CHECK_EQ(resolveAlias("@test", workspace.fileResolver.defaultConfig), std::filesystem::current_path() / "folder");
    CHECK_EQ(resolveAlias("@test/", workspace.fileResolver.defaultConfig), std::filesystem::current_path() / "folder");
    CHECK_EQ(resolveAlias("@test/foo", workspace.fileResolver.defaultConfig), std::filesystem::current_path() / "folder/foo");
}

TEST_CASE_FIXTURE(Fixture, "resolve_alias_handles_if_alias_was_defined_with_trailing_slash")
{
    loadLuaurc(R"(
    {
        "aliases": {
            "test": "folder/"
        }
    }
    )");

    CHECK_EQ(resolveAlias("@test", workspace.fileResolver.defaultConfig), std::filesystem::current_path() / "folder/");
    CHECK_EQ(resolveAlias("@test/", workspace.fileResolver.defaultConfig), std::filesystem::current_path() / "folder/");
    CHECK_EQ(resolveAlias("@test/foo", workspace.fileResolver.defaultConfig), std::filesystem::current_path() / "folder/foo");
}

TEST_CASE_FIXTURE(Fixture, "resolve_alias_supports_absolute_paths")
{
#ifdef _WIN32
    auto basePath = "C:/Users/test/folder";
#else
    auto basePath = "/home/folder";
#endif

    std::string source = R"(
        {
            "aliases": {
                "test": "{basePath}"
            }
        }
    )";
    replace(source, "{basePath}", basePath);
    loadLuaurc(source);

    CHECK_EQ(resolveAlias("@test", workspace.fileResolver.defaultConfig), basePath);
    CHECK_EQ(resolveAlias("@test/", workspace.fileResolver.defaultConfig), basePath);
    CHECK_EQ(resolveAlias("@test/foo", workspace.fileResolver.defaultConfig), std::string(basePath) + "/foo");
}

TEST_CASE_FIXTURE(Fixture, "resolve_alias_supports_tilde_expansion")
{
    loadLuaurc(R"(
    {
        "aliases": {
            "test": "~/definitions"
        }
    }
    )");

    auto home = getHomeDirectory();
    REQUIRE(home);

    CHECK_EQ(resolveAlias("@test", workspace.fileResolver.defaultConfig), *home / "definitions");
    CHECK_EQ(resolveAlias("@test/", workspace.fileResolver.defaultConfig), *home / "definitions");
    CHECK_EQ(resolveAlias("@test/foo", workspace.fileResolver.defaultConfig), *home / "definitions" / "foo");
}

TEST_CASE_FIXTURE(Fixture, "string require doesn't add file extension if already exists")
{
    WorkspaceFileResolver fileResolver;
    LSPPlatform platform{&fileResolver};
    fileResolver.platform = &platform;

    Luau::ModuleInfo baseContext{};
    auto resolved = platform.resolveStringRequire(&baseContext, "Module.luau");

    REQUIRE(resolved.has_value());
    CHECK(endsWith(resolved->name, "/Module.luau"));
}

TEST_CASE_FIXTURE(Fixture, "string require doesn't replace a non-luau/lua extension")
{
    WorkspaceFileResolver fileResolver;
    LSPPlatform platform{&fileResolver};
    fileResolver.platform = &platform;

    Luau::ModuleInfo baseContext{};
    auto resolved = platform.resolveStringRequire(&baseContext, "Module.mod");

    REQUIRE(resolved.has_value());
    CHECK(endsWith(resolved->name, "/Module.mod.lua"));
}

TEST_CASE_FIXTURE(Fixture, "string_require_resolves_relative_to_file")
{
    auto moduleName = "tests/testdata/requires/relative_to_file/main.luau";
    auto result = workspace.frontend.check(moduleName);

    LUAU_LSP_REQUIRE_NO_ERRORS(result);

    CHECK_EQ(Luau::toString(requireType(getModule(moduleName), "other")), "number");
}

TEST_CASE_FIXTURE(Fixture, "resolve_json_modules")
{
    TempDir t("resolve_json_modules");
    auto path = t.write_child("settings.json", R"({"value": 1})");

    auto sourcemap = std::string(R"(
    {
        "name": "Game",
        "className": "DataModel",
        "children": [{ "name": "Settings", "className": "ModuleScript", "filePaths": ["{filepath}"] }]
    }
    )");
    replace(sourcemap, "{filepath}", path);
    loadSourcemap(sourcemap);

    auto source = workspace.fileResolver.readSource("game/Settings");
    REQUIRE(source);

    CHECK_EQ(source->source, "--!strict\nreturn {[\"value\"] = 1;}");
}

TEST_CASE_FIXTURE(Fixture, "resolve_toml_modules")
{
    TempDir t("resolve_toml_modules");
    auto path = t.write_child("settings.toml", R"(value = 1)");

    auto sourcemap = std::string(R"(
    {
        "name": "Game",
        "className": "DataModel",
        "children": [{ "name": "Settings", "className": "ModuleScript", "filePaths": ["{filepath}"] }]
    }
    )");
    replace(sourcemap, "{filepath}", path);
    loadSourcemap(sourcemap);

    auto source = workspace.fileResolver.readSource("game/Settings");
    REQUIRE(source);

    CHECK_EQ(source->source, "--!strict\nreturn {[\"value\"] = 1;}");
}

TEST_SUITE_END();
