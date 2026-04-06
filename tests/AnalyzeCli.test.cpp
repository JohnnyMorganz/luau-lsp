#include "doctest.h"
#include "TempDir.h"
#include "Fixture.h"
#include "Analyze/AnalyzeCli.hpp"
#include "LSP/WorkspaceFileResolver.hpp"
#include "LSP/FileConfiguration.hpp"
#include "Analyze/CliConfigurationParser.hpp"

namespace std
{
template<typename T>
ostream& operator<<(ostream& os, const vector<T>& value)
{
    os << "[";
    bool first = true;
    for (const auto& path : value)
    {
        if (!first)
            os << ", ";
        else
            first = false;
        os << path;
    }
    os << "]";
    return os;
}
} // namespace std

TEST_CASE("getFilesToAnalyze")
{
    TempDir t("analyze_cli_get_files_to_analyze");
    auto fileA = Uri::file(t.write_child("src/a.luau", "")).fsPath();
    auto fileB = Uri::file(t.write_child("src/b.luau", "")).fsPath();

    auto allResults = getFilesToAnalyze({t.path()}, {});
    std::sort(allResults.begin(), allResults.end());
    CHECK_EQ(allResults, std::vector<std::string>{fileA, fileB});

    auto ignoredFile = getFilesToAnalyze({t.path()}, {"b.luau"});
    std::sort(ignoredFile.begin(), ignoredFile.end());
    CHECK_EQ(ignoredFile, std::vector<std::string>{fileA});
}

TEST_CASE("getFilesToAnalyze_handles_ignore_globs_within_directories")
{
    TempDir t("analyze_cli_ignored_files");
    auto fileA = Uri::file(t.write_child("src/a.luau", "")).fsPath();
    auto fileB = Uri::file(t.write_child("src/b.luau", "")).fsPath();

    auto ignoredFile = getFilesToAnalyze({t.path()}, {"b.luau"});
    std::sort(ignoredFile.begin(), ignoredFile.end());
    CHECK_EQ(ignoredFile, std::vector<std::string>{fileA});
}

TEST_CASE("getFilesToAnalyze_still_matches_file_if_it_was_explicitly_provided")
{
    TempDir t("analyze_cli_ignored_files_explicitly_provided");
    auto fileA = Uri::file(t.write_child("src/a.luau", "")).fsPath();
    CHECK_EQ(getFilesToAnalyze({fileA}, {"a.luau"}), std::vector<std::string>{fileA});
}

TEST_CASE("ignore_globs_from_settings_file_applied")
{
    CliClient client;
    std::vector<std::string> ignoreGlobs;
    std::unordered_map<std::string, std::string> definitionPaths;

    auto configFile = R"({ "luau-lsp.ignoreGlobs": [ "/ignored/**" ] })";

    applySettings(configFile, client, ignoreGlobs, definitionPaths);

    REQUIRE_EQ(ignoreGlobs.size(), 1);
    CHECK_EQ(ignoreGlobs[0], "/ignored/**");
}

TEST_CASE("definition_files_from_settings_file_applied")
{
    CliClient client;
    std::vector<std::string> ignoreGlobs;
    std::unordered_map<std::string, std::string> definitionPaths;

    auto configFile = R"({ "luau-lsp.types.definitionFiles": [ "global_types/types.d.luau" ] })";

    applySettings(configFile, client, ignoreGlobs, definitionPaths);

    REQUIRE_EQ(definitionPaths.size(), 1);
    REQUIRE(definitionPaths.find("@roblox1") != definitionPaths.end());
    CHECK_EQ(definitionPaths["@roblox1"], "global_types/types.d.luau");
}

TEST_CASE("enable_new_solver_fflag_from_settings_file_applied")
{
    ScopedFastFlag sff{FFlag::LuauSolverV2, false};

    CliClient client;
    std::vector<std::string> ignoreGlobs;
    std::unordered_map<std::string, std::string> definitionPaths;

    auto configFile = R"({ "luau-lsp.fflags.enableNewSolver": true })";

    applySettings(configFile, client, ignoreGlobs, definitionPaths);

    CHECK(FFlag::LuauSolverV2);
}

TEST_CASE_FIXTURE(Fixture, "analysis_relative_file_paths")
{
    CHECK_EQ(getFilePath(&workspace.fileResolver, tempDir.touch_child("test.luau")).relativePath, "test.luau");
    CHECK_EQ(getFilePath(&workspace.fileResolver, tempDir.touch_child("folder/file.luau")).relativePath, "folder/file.luau");
}

TEST_CASE("parse_definitions_files_handles_new_syntax")
{
    argparse::ArgumentParser program("test");
    program.set_assign_chars(":=");
    program.add_argument("--definitions", "--defs")
        .help("A path to a Luau definitions file to load into the global namespace")
        .default_value<std::vector<std::string>>({})
        .append()
        .metavar("PATH");

    std::vector<std::string> arguments{
        "", "--definitions:@roblox=example_path.d.luau", "--definitions:@lune=lune.d.luau", "--definitions:no_at_sign=path.d.luau"};
    program.parse_args(arguments);

    auto definitionsFiles = processDefinitionsFilePaths(program);

    CHECK_EQ(definitionsFiles, std::unordered_map<std::string, std::string>{
                                   {"@roblox", "example_path.d.luau"},
                                   {"@lune", "lune.d.luau"},
                                   {"@no_at_sign", "path.d.luau"},
                               });
}

TEST_CASE("parse_definitions_files_handles_legacy_syntax")
{
    argparse::ArgumentParser program("test");
    program.set_assign_chars(":=");
    program.add_argument("--definitions", "--defs")
        .help("A path to a Luau definitions file to load into the global namespace")
        .default_value<std::vector<std::string>>({})
        .append()
        .metavar("PATH");

    std::vector<std::string> arguments{"", "--definitions=example_path.d.luau", "--definitions=lune.d.luau"};
    program.parse_args(arguments);

    auto definitionsFiles = processDefinitionsFilePaths(program);

    CHECK_EQ(definitionsFiles, std::unordered_map<std::string, std::string>{
                                   {"@roblox", "example_path.d.luau"},
                                   {"@roblox1", "lune.d.luau"},
                               });
}

TEST_CASE("applyFileConfiguration_sets_platform")
{
    CliClient client;
    std::vector<std::string> ignoreGlobs;
    std::unordered_map<std::string, std::string> definitionPaths;

    FileConfiguration fileConfig;
    fileConfig.platform.type = LSPPlatformConfig::Roblox;

    applyFileConfiguration(fileConfig, client, definitionPaths);

    CHECK_EQ(client.configuration.platform.type, LSPPlatformConfig::Roblox);
}

TEST_CASE("applyFileConfiguration_sets_definition_files")
{
    CliClient client;
    std::vector<std::string> ignoreGlobs;
    std::unordered_map<std::string, std::string> definitionPaths;

    FileConfiguration fileConfig;
    fileConfig.types.definitionFiles = std::unordered_map<std::string, std::string>{
        {"testez", "/project/types/testez.d.luau"},
        {"@lune", "/project/types/lune.d.luau"},
    };

    applyFileConfiguration(fileConfig, client, definitionPaths);

    CHECK_EQ(definitionPaths.size(), 2);
    CHECK_EQ(definitionPaths.at("@testez"), "/project/types/testez.d.luau");
    CHECK_EQ(definitionPaths.at("@lune"), "/project/types/lune.d.luau");
}

TEST_CASE("applyFileConfiguration_sets_sourcemap")
{
    CliClient client;
    std::vector<std::string> ignoreGlobs;
    std::unordered_map<std::string, std::string> definitionPaths;

    FileConfiguration fileConfig;
    fileConfig.sourcemap.rojoProjectFile = "/project/default.project.json";
    fileConfig.sourcemap.sourcemapFile = "/project/sourcemap.json";

    applyFileConfiguration(fileConfig, client, definitionPaths);

    CHECK_EQ(client.configuration.sourcemap.rojoProjectFile, "/project/default.project.json");
    CHECK_EQ(client.configuration.sourcemap.sourcemapFile, "/project/sourcemap.json");
}

TEST_CASE("applyFileConfiguration_does_not_override_existing_definition_paths")
{
    CliClient client;
    std::vector<std::string> ignoreGlobs;
    std::unordered_map<std::string, std::string> definitionPaths;
    definitionPaths["@roblox"] = "cli/roblox.d.luau";

    FileConfiguration fileConfig;
    fileConfig.types.definitionFiles = std::unordered_map<std::string, std::string>{
        {"@roblox", "/project/types/roblox.d.luau"},
        {"extra", "/project/types/extra.d.luau"},
    };

    applyFileConfiguration(fileConfig, client, definitionPaths);

    // CLI-provided @roblox should NOT be overwritten (emplace doesn't overwrite)
    CHECK_EQ(definitionPaths.at("@roblox"), "cli/roblox.d.luau");
    // New entry should be added
    CHECK_EQ(definitionPaths.at("@extra"), "/project/types/extra.d.luau");
}

TEST_SUITE_END();
