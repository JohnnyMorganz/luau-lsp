#include "doctest.h"
#include "Fixture.h"
#include "LSP/WorkspaceFileResolver.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "Luau/Ast.h"
#include "Luau/FileResolver.h"
#include "TempDir.h"
#include "LuauFileUtils.hpp"

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

    auto rootPath = Uri::file("");
    auto home = getHomeDirectory();
    REQUIRE(home);

    CHECK_EQ(resolveDirectoryAlias(rootPath, directoryAliases, "@test1/foo"), Uri::file("C:/Users/test/test1/foo"));
    CHECK_EQ(resolveDirectoryAlias(rootPath, directoryAliases, "@test1/foo.luau"), Uri::file("C:/Users/test/test1/foo.luau"));
    CHECK_EQ(resolveDirectoryAlias(rootPath, directoryAliases, "@test1/"), Uri::file("C:/Users/test/test1"));
    // NOTE: we do not strip `/` from `@test1`, so we use it as `@test4`
    // for now we don't "fix" this, because our startsWith check is greedy, so we want to allow differentiation between `@foo/` and `@foobar/`
    CHECK_EQ(resolveDirectoryAlias(rootPath, directoryAliases, "@test4"), Uri::file("C:/Users/test/test1"));

    CHECK_EQ(resolveDirectoryAlias(rootPath, directoryAliases, "@test2/bar"), Uri::file(Luau::FileUtils::joinPaths(*home, "test2/bar")));

    CHECK_EQ(resolveDirectoryAlias(rootPath, directoryAliases, "@test3/bar"), std::nullopt);

    // Relative directory aliases
    CHECK_EQ(resolveDirectoryAlias(Uri::file("workspace"), directoryAliases, "@relative/foo.luau"), Uri::file("workspace/src/client/foo.luau"));
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

    CHECK_EQ(resolveAlias("test", workspace.fileResolver.defaultConfig, {}), std::nullopt);
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

    CHECK_EQ(resolveAlias("@test", workspace.fileResolver.defaultConfig, {}), workspace.fileResolver.rootUri.resolvePath("folder"));
    CHECK_EQ(resolveAlias("@test/", workspace.fileResolver.defaultConfig, {}), workspace.fileResolver.rootUri.resolvePath("folder"));
    CHECK_EQ(resolveAlias("@test/foo", workspace.fileResolver.defaultConfig, {}), workspace.fileResolver.rootUri.resolvePath("folder/foo"));
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

    auto x = resolveAlias("@test", workspace.fileResolver.defaultConfig, {});

    CHECK_EQ(resolveAlias("@test", workspace.fileResolver.defaultConfig, {}), workspace.fileResolver.rootUri.resolvePath("folder"));
    CHECK_EQ(resolveAlias("@test/", workspace.fileResolver.defaultConfig, {}), workspace.fileResolver.rootUri.resolvePath("folder"));
    CHECK_EQ(resolveAlias("@test/foo", workspace.fileResolver.defaultConfig, {}), workspace.fileResolver.rootUri.resolvePath("folder/foo"));
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

    CHECK_EQ(resolveAlias("@test", workspace.fileResolver.defaultConfig, {}), Uri::file(basePath));
    CHECK_EQ(resolveAlias("@test/", workspace.fileResolver.defaultConfig, {}), Uri::file(basePath));
    CHECK_EQ(resolveAlias("@test/foo", workspace.fileResolver.defaultConfig, {}), Uri::file(basePath).resolvePath("foo"));
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

    CHECK_EQ(resolveAlias("@test", workspace.fileResolver.defaultConfig, {}), Uri::file(Luau::FileUtils::joinPaths(*home, "definitions")));
    CHECK_EQ(resolveAlias("@test/", workspace.fileResolver.defaultConfig, {}), Uri::file(Luau::FileUtils::joinPaths(*home, "definitions")));
    CHECK_EQ(resolveAlias("@test/foo", workspace.fileResolver.defaultConfig, {}), Uri::file(Luau::FileUtils::joinPaths(*home, "definitions/foo")));
}

TEST_CASE_FIXTURE(Fixture, "resolve_alias_supports_self_alias")
{
    auto basePath = workspace.fileResolver.rootUri;

    CHECK_EQ(resolveAlias("@self", workspace.fileResolver.defaultConfig, basePath), basePath);
    CHECK_EQ(resolveAlias("@self/foo", workspace.fileResolver.defaultConfig, basePath), basePath.resolvePath("foo"));
}

TEST_CASE_FIXTURE(Fixture, "string require doesn't add file extension if already exists")
{
    Luau::ModuleInfo baseContext{workspace.fileResolver.getModuleName(workspace.rootUri)};
    auto resolved = workspace.platform->resolveStringRequire(&baseContext, "Module.luau");

    REQUIRE(resolved.has_value());
#ifdef _WIN32
    CHECK(endsWith(resolved->name, "\\Module.luau"));
#else
    CHECK(endsWith(resolved->name, "/Module.luau"));
#endif
}

TEST_CASE_FIXTURE(Fixture, "string require doesn't replace a non-luau/lua extension")
{
    Luau::ModuleInfo baseContext{workspace.fileResolver.getModuleName(workspace.rootUri)};
    auto resolved = workspace.platform->resolveStringRequire(&baseContext, "Module.mod");

    REQUIRE(resolved.has_value());
#ifdef _WIN32
    CHECK(endsWith(resolved->name, "\\Module.mod.lua"));
#else
    CHECK(endsWith(resolved->name, "/Module.mod.lua"));
#endif
}

TEST_CASE_FIXTURE(Fixture, "string_require_resolves_relative_to_file_integration_test")
{
    auto moduleName = workspace.fileResolver.getModuleName(workspace.rootUri.resolvePath("tests/testdata/requires/relative_to_file/main.luau"));
    auto result = workspace.frontend.check(moduleName);

    LUAU_LSP_REQUIRE_NO_ERRORS(result);

    CHECK_EQ(Luau::toString(requireType(getModule(moduleName), "other")), "number");
}

TEST_CASE_FIXTURE(Fixture, "string_require_resolves_relative_to_file")
{
    TempDir t("string_require_relative_to_file");
    auto projectLibMainPath = t.touch_child("project/lib/main.luau");
    auto projectLibUtilsPath = t.touch_child("project/lib/utils.luau");
    auto projectOtherPath = t.touch_child("project/other.luau");

    Luau::ModuleInfo baseContext{projectLibMainPath};
    CHECK_EQ(Uri::file(workspace.fileResolver.platform->resolveStringRequire(&baseContext, "./utils")->name), Uri::file(projectLibUtilsPath));
    CHECK_EQ(Uri::file(workspace.fileResolver.platform->resolveStringRequire(&baseContext, "../other")->name), Uri::file(projectOtherPath));
}

TEST_CASE_FIXTURE(Fixture, "string_require_resolves_a_directory_as_the_init_luau_file")
{
    TempDir t("string_require_resolve_directory_as_init");
    auto projectLibMainPath = t.touch_child("project/lib/main.luau");
    auto projectOtherPath = t.touch_child("project/other/init.luau");

    Luau::ModuleInfo baseContext{projectLibMainPath};
    CHECK_EQ(Uri::file(workspace.fileResolver.platform->resolveStringRequire(&baseContext, "../other")->name), Uri::file(projectOtherPath));
}

TEST_CASE_FIXTURE(Fixture, "string_require_resolves_relative_to_directory_for_init_luau")
{
    TempDir t("resolve_init_luau_relative_to_directory");
    auto toolsInitPath = t.touch_child("tools/init.luau");
    auto toolsFilePath = t.touch_child("tools/file.luau");
    auto projectSiblingPath = t.touch_child("project/sibling.luau");
    auto projectDirectoryUtilsPath = t.touch_child("project/directory/utils.luau");
    auto projectDirectoryInitPath = t.touch_child("project/directory/init.luau");

    Luau::ModuleInfo baseContext{projectDirectoryInitPath};
    CHECK_EQ(Uri::file(workspace.fileResolver.platform->resolveStringRequire(&baseContext, "./sibling")->name), Uri::file(projectSiblingPath));

    CHECK_EQ(Uri::file(workspace.fileResolver.platform->resolveStringRequire(&baseContext, "../tools")->name), Uri::file(toolsInitPath));
    CHECK_EQ(Uri::file(workspace.fileResolver.platform->resolveStringRequire(&baseContext, "../tools/file")->name), Uri::file(toolsFilePath));

    CHECK_EQ(
        Uri::file(workspace.fileResolver.platform->resolveStringRequire(&baseContext, "./utils")->name), Uri::file(t.path() + "/project/utils.lua"));
    CHECK_EQ(Uri::file(workspace.fileResolver.platform->resolveStringRequire(&baseContext, "./directory/utils")->name),
        Uri::file(projectDirectoryUtilsPath));
}

TEST_CASE_FIXTURE(Fixture, "string_require_resolve_file_named_luau")
{
    TempDir t("resolve_file_named_luau");
    auto mainPath = t.touch_child("project/main.luau");
    auto luauPath = t.touch_child("project/luau.luau");

    Luau::ModuleInfo baseContext{mainPath};
    CHECK_EQ(Uri::file(workspace.fileResolver.platform->resolveStringRequire(&baseContext, "./luau")->name), Uri::file(luauPath));
}

TEST_CASE("is_init_luau_file")
{
    CHECK_EQ(isInitLuauFile(Uri::file("foo/init.lua")), true);
    CHECK_EQ(isInitLuauFile(Uri::file("foo/init.luau")), true);
    CHECK_EQ(isInitLuauFile(Uri::file("foo/init.client.luau")), true);
    CHECK_EQ(isInitLuauFile(Uri::file("foo/init.server.luau")), true);

    CHECK_EQ(isInitLuauFile(Uri::file("foo/utils.luau")), false);
}

TEST_CASE_FIXTURE(Fixture, "string_require_resolves_self_alias")
{
    TempDir t("resolve_init_luau_self_alias");
    auto projectInitPath = t.touch_child("project/init.luau");
    auto projectUtilsPath = t.touch_child("project/utils.luau");

    Luau::ModuleInfo baseContext{projectInitPath};
    CHECK_EQ(Uri::file(workspace.fileResolver.platform->resolveStringRequire(&baseContext, "@self/utils")->name), Uri::file(projectUtilsPath));
}

TEST_CASE_FIXTURE(Fixture, "init_luau_files_should_not_be_aware_of_sibling_luaurc_files")
{
    TempDir t("init_luau_files_should_not_be_aware_of_sibling_luaurc_files");
    auto initPath = t.touch_child("project/code/init.luau");
    auto siblingFilePath = t.touch_child("project/code/sibling.luau");
    auto luaurcPath = t.write_child("project/code/.luaurc", R"({
        "aliases": {
            "test": "test"
        }
    })");

    auto initConfig = workspace.fileResolver.getConfig(workspace.fileResolver.getModuleName(Uri::file(initPath)));
    auto siblingConfig = workspace.fileResolver.getConfig(workspace.fileResolver.getModuleName(Uri::file(siblingFilePath)));

    CHECK_EQ(initConfig.aliases.size(), 0);
    CHECK_EQ(siblingConfig.aliases.size(), 1);
    CHECK(siblingConfig.aliases.find("test"));
}

TEST_CASE_FIXTURE(Fixture, "init_luau_files_are_aware_of_luaurc_files_that_are_sibling_to_its_parent_directory")
{
    TempDir t("init_luau_files_are_aware_of_luaurc_files_that_are_sibling_to_its_parent_directory");
    auto initPath = t.touch_child("project/code/init.luau");
    auto luaurcPath = t.write_child("project/.luaurc", R"({
        "aliases": {
            "test": "test"
        }
    })");

    auto initConfig = workspace.fileResolver.getConfig(workspace.fileResolver.getModuleName(Uri::file(initPath)));

    CHECK_EQ(initConfig.aliases.size(), 1);
    CHECK(initConfig.aliases.find("test"));
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
