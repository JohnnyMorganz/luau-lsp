#include "doctest.h"
#include "LSP/FileConfiguration.hpp"
#include "Luau/LuauConfig.h"

static Luau::ConfigTable makeConfigTable(const std::string& source)
{
    std::string error;
    auto table = Luau::extractConfig(source, {}, &error);
    REQUIRE_MESSAGE(table.has_value(), error);
    return std::move(*table);
}

TEST_SUITE_BEGIN("FileConfiguration");

TEST_CASE("extractLspConfigFromTable_no_lsp_key")
{
    auto table = makeConfigTable(R"(return { languagemode = "strict" })");

    FileConfiguration result;
    auto error = extractLspConfigFromTable(table, result, "/project");
    CHECK_FALSE(error.has_value());
    CHECK_FALSE(result.hasAnyValue());
}

TEST_CASE("extractLspConfigFromTable_empty_lsp_table")
{
    auto table = makeConfigTable(R"(return { lsp = {} })");

    FileConfiguration result;
    auto error = extractLspConfigFromTable(table, result, "/project");
    CHECK_FALSE(error.has_value());
    CHECK_FALSE(result.hasAnyValue());
}

TEST_CASE("extractLspConfigFromTable_platform_roblox")
{
    auto table = makeConfigTable(R"(return { lsp = { platform = { type = "roblox" } } })");

    FileConfiguration result;
    auto error = extractLspConfigFromTable(table, result, "/project");
    CHECK_FALSE(error.has_value());
    REQUIRE(result.platform.type.has_value());
    CHECK_EQ(*result.platform.type, LSPPlatformConfig::Roblox);
}

TEST_CASE("extractLspConfigFromTable_platform_standard")
{
    auto table = makeConfigTable(R"(return { lsp = { platform = { type = "standard" } } })");

    FileConfiguration result;
    auto error = extractLspConfigFromTable(table, result, "/project");
    CHECK_FALSE(error.has_value());
    REQUIRE(result.platform.type.has_value());
    CHECK_EQ(*result.platform.type, LSPPlatformConfig::Standard);
}

TEST_CASE("extractLspConfigFromTable_platform_invalid_type")
{
    auto table = makeConfigTable(R"(return { lsp = { platform = { type = "invalid" } } })");

    FileConfiguration result;
    auto error = extractLspConfigFromTable(table, result, "/project");
    REQUIRE(error.has_value());
    CHECK(error->find("\"standard\" or \"roblox\"") != std::string::npos);
}

TEST_CASE("extractLspConfigFromTable_platform_type_not_string")
{
    auto table = makeConfigTable(R"(return { lsp = { platform = { type = 123 } } })");

    FileConfiguration result;
    auto error = extractLspConfigFromTable(table, result, "/project");
    REQUIRE(error.has_value());
    CHECK(error->find("must be a string") != std::string::npos);
}

TEST_CASE("extractLspConfigFromTable_lsp_not_a_table")
{
    auto table = makeConfigTable(R"(return { lsp = "not a table" })");

    FileConfiguration result;
    auto error = extractLspConfigFromTable(table, result, "/project");
    REQUIRE(error.has_value());
    CHECK(error->find("\"lsp\" must be a table") != std::string::npos);
}

TEST_CASE("extractLspConfigFromTable_platform_not_a_table")
{
    auto table = makeConfigTable(R"(return { lsp = { platform = "bad" } })");

    FileConfiguration result;
    auto error = extractLspConfigFromTable(table, result, "/project");
    REQUIRE(error.has_value());
    CHECK(error->find("\"lsp.platform\" must be a table") != std::string::npos);
}

TEST_CASE("extractLspConfigFromTable_sourcemap_field_not_string")
{
    auto table = makeConfigTable(R"(return { lsp = { sourcemap = { rojoProjectFile = 123 } } })");

    FileConfiguration result;
    auto error = extractLspConfigFromTable(table, result, "/project");
    REQUIRE(error.has_value());
    CHECK(error->find("must be a string") != std::string::npos);
}

TEST_CASE("extractLspConfigFromTable_sourcemap")
{
    auto table = makeConfigTable(R"(return {
        lsp = {
            sourcemap = {
                rojoProjectFile = "default.project.json",
                sourcemapFile = "sourcemap.json",
            },
        },
    })");

    FileConfiguration result;
    auto error = extractLspConfigFromTable(table, result, "/project");
    CHECK_FALSE(error.has_value());
    REQUIRE(result.sourcemap.rojoProjectFile.has_value());
    REQUIRE(result.sourcemap.sourcemapFile.has_value());
    CHECK_EQ(*result.sourcemap.rojoProjectFile, "/project/default.project.json");
    CHECK_EQ(*result.sourcemap.sourcemapFile, "/project/sourcemap.json");
}

TEST_CASE("extractLspConfigFromTable_sourcemap_absolute_paths")
{
    auto table = makeConfigTable(R"(return {
        lsp = {
            sourcemap = {
                rojoProjectFile = "/absolute/path/project.json",
            },
        },
    })");

    FileConfiguration result;
    auto error = extractLspConfigFromTable(table, result, "/project");
    CHECK_FALSE(error.has_value());
    REQUIRE(result.sourcemap.rojoProjectFile.has_value());
    CHECK_EQ(*result.sourcemap.rojoProjectFile, "/absolute/path/project.json");
}

TEST_CASE("extractLspConfigFromTable_types_definitionFiles")
{
    auto table = makeConfigTable(R"(return {
        lsp = {
            types = {
                definitionFiles = {
                    testez = "types/testez.d.luau",
                    custom = "types/custom.d.luau",
                },
            },
        },
    })");

    FileConfiguration result;
    auto error = extractLspConfigFromTable(table, result, "/project");
    CHECK_FALSE(error.has_value());
    REQUIRE(result.types.definitionFiles.has_value());
    auto& defFiles = *result.types.definitionFiles;
    CHECK_EQ(defFiles.size(), 2);
    CHECK_EQ(defFiles.at("testez"), "/project/types/testez.d.luau");
    CHECK_EQ(defFiles.at("custom"), "/project/types/custom.d.luau");
}

TEST_CASE("extractLspConfigFromTable_types_disabledGlobals")
{
    auto table = makeConfigTable(R"(return {
        lsp = {
            types = {
                disabledGlobals = { "table.freeze", "game" },
            },
        },
    })");

    FileConfiguration result;
    auto error = extractLspConfigFromTable(table, result, "/project");
    CHECK_FALSE(error.has_value());
    REQUIRE(result.types.disabledGlobals.has_value());
    auto& globals = *result.types.disabledGlobals;
    CHECK_EQ(globals.size(), 2);
    CHECK_EQ(globals[0], "table.freeze");
    CHECK_EQ(globals[1], "game");
}

TEST_CASE("extractLspConfigFromTable_unknown_keys_ignored")
{
    auto table = makeConfigTable(R"(return {
        lsp = {
            unknownSection = { foo = "bar" },
            platform = { type = "roblox" },
        },
    })");

    FileConfiguration result;
    auto error = extractLspConfigFromTable(table, result, "/project");
    CHECK_FALSE(error.has_value());
    REQUIRE(result.platform.type.has_value());
    CHECK_EQ(*result.platform.type, LSPPlatformConfig::Roblox);
}

TEST_CASE("extractLspConfigFromTable_full_config")
{
    auto table = makeConfigTable(R"(return {
        languagemode = "strict",
        lsp = {
            platform = { type = "roblox" },
            sourcemap = {
                rojoProjectFile = "default.project.json",
                sourcemapFile = "sourcemap.json",
            },
            types = {
                definitionFiles = { testez = "types/testez.d.luau" },
                disabledGlobals = { "table.freeze" },
            },
        },
    })");

    FileConfiguration result;
    auto error = extractLspConfigFromTable(table, result, "/project");
    CHECK_FALSE(error.has_value());
    CHECK(result.hasAnyValue());
    CHECK_EQ(*result.platform.type, LSPPlatformConfig::Roblox);
    CHECK_EQ(*result.sourcemap.rojoProjectFile, "/project/default.project.json");
    CHECK_EQ(*result.sourcemap.sourcemapFile, "/project/sourcemap.json");
    CHECK_EQ(result.types.definitionFiles->at("testez"), "/project/types/testez.d.luau");
    CHECK_EQ(result.types.disabledGlobals->at(0), "table.freeze");
}

TEST_CASE("mergeConfigurations_no_file_config_values")
{
    ClientConfiguration editorConfig;
    editorConfig.platform.type = LSPPlatformConfig::Standard;
    editorConfig.sourcemap.rojoProjectFile = "editor.project.json";

    FileConfiguration fileConfig; // all empty

    auto merged = mergeConfigurations(editorConfig, fileConfig);
    CHECK_EQ(merged.platform.type, LSPPlatformConfig::Standard);
    CHECK_EQ(merged.sourcemap.rojoProjectFile, "editor.project.json");
}

TEST_CASE("mergeConfigurations_file_overrides_editor")
{
    ClientConfiguration editorConfig;
    editorConfig.platform.type = LSPPlatformConfig::Standard;
    editorConfig.sourcemap.rojoProjectFile = "editor.project.json";
    editorConfig.sourcemap.sourcemapFile = "editor-sourcemap.json";

    FileConfiguration fileConfig;
    fileConfig.platform.type = LSPPlatformConfig::Roblox;
    fileConfig.sourcemap.rojoProjectFile = "file.project.json";

    auto merged = mergeConfigurations(editorConfig, fileConfig);
    CHECK_EQ(merged.platform.type, LSPPlatformConfig::Roblox);
    CHECK_EQ(merged.sourcemap.rojoProjectFile, "file.project.json");
    // sourcemapFile not set in file config, should keep editor value
    CHECK_EQ(merged.sourcemap.sourcemapFile, "editor-sourcemap.json");
}

TEST_CASE("mergeConfigurations_file_overrides_types")
{
    ClientConfiguration editorConfig;
    editorConfig.types.definitionFiles = {{std::string("pkg1"), std::string("editor/path.d.luau")}};
    editorConfig.types.disabledGlobals = {"editorGlobal"};

    FileConfiguration fileConfig;
    fileConfig.types.definitionFiles = std::unordered_map<std::string, std::string>{{"pkg2", "file/path.d.luau"}};

    auto merged = mergeConfigurations(editorConfig, fileConfig);
    // definitionFiles fully replaced by file config
    CHECK_EQ(merged.types.definitionFiles.size(), 1);
    CHECK_EQ(merged.types.definitionFiles.at("pkg2"), "file/path.d.luau");
    // disabledGlobals not set in file config, keeps editor value
    CHECK_EQ(merged.types.disabledGlobals.size(), 1);
    CHECK_EQ(merged.types.disabledGlobals[0], "editorGlobal");
}

TEST_CASE("mergeConfigurations_preserves_non_project_settings")
{
    ClientConfiguration editorConfig;
    editorConfig.hover.enabled = false;
    editorConfig.completion.addParentheses = false;
    editorConfig.diagnostics.workspace = true;

    FileConfiguration fileConfig;
    fileConfig.platform.type = LSPPlatformConfig::Roblox;

    auto merged = mergeConfigurations(editorConfig, fileConfig);
    // Non-project settings unchanged
    CHECK_EQ(merged.hover.enabled, false);
    CHECK_EQ(merged.completion.addParentheses, false);
    CHECK_EQ(merged.diagnostics.workspace, true);
    // Project setting overridden
    CHECK_EQ(merged.platform.type, LSPPlatformConfig::Roblox);
}

TEST_SUITE_END();
