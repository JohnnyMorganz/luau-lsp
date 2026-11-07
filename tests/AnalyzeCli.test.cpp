#include "doctest.h"
#include "TempDir.h"
#include "Fixture.h"
#include "Analyze/AnalyzeCli.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/WorkspaceFileResolver.hpp"
#include "Analyze/CliConfigurationParser.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "Luau/TypeAttach.h"
#include "Luau/PrettyPrinter.h"

LUAU_FASTFLAG(DebugLuauMagicTypes)

TEST_SUITE_BEGIN("AnalyzeCli");

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

/// Sets up default client config and definitions. Call this before customizing
/// globalConfig fields, then call setupCliWorkspace() to finalize.
static void initCliClient(CliClient& client)
{
    client.globalConfig = Luau::LanguageServer::defaultTestClientConfiguration();
    client.definitionsFiles.emplace("@roblox", "./tests/testdata/standard_definitions.d.luau");
}

static void setupCliWorkspace(CliClient& client, WorkspaceFolder& workspace)
{
    workspace.setupWithConfiguration(client.globalConfig);
    workspace.isReady = true;
}

static void initRobloxCliClient(CliClient& client)
{
    initCliClient(client);
    client.globalConfig.platform.type = LSPPlatformConfig::Roblox;
    client.globalConfig.sourcemap.enabled = true;
    client.globalConfig.sourcemap.sourcemapFile = "sourcemap.json";
}

TEST_CASE("getFilesToAnalyze")
{
    TempDir t("analyze_cli_get_files_to_analyze");
    auto fileA = Uri::file(t.write_child("src/a.luau", "")).fsPath();
    auto fileB = Uri::file(t.write_child("src/b.luau", "")).fsPath();

    auto allResults = getFilesToAnalyze({t.path()});
    std::sort(allResults.begin(), allResults.end());
    CHECK_EQ(allResults, std::vector<std::string>{fileA, fileB});

    CliClient client;
    initCliClient(client);
    client.globalConfig.ignoreGlobs = {"b.luau"};
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto ignoredFile = getFilesToAnalyze({t.path()}, &workspace);
    std::sort(ignoredFile.begin(), ignoredFile.end());
    CHECK_EQ(ignoredFile, std::vector<std::string>{fileA});
}

TEST_CASE("getFilesToAnalyze_handles_ignore_globs_within_directories")
{
    TempDir t("analyze_cli_ignored_files");
    auto fileA = Uri::file(t.write_child("src/a.luau", "")).fsPath();
    auto fileB = Uri::file(t.write_child("src/b.luau", "")).fsPath();

    CliClient client;
    initCliClient(client);
    client.globalConfig.ignoreGlobs = {"b.luau"};
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto ignoredFile = getFilesToAnalyze({t.path()}, &workspace);
    std::sort(ignoredFile.begin(), ignoredFile.end());
    CHECK_EQ(ignoredFile, std::vector<std::string>{fileA});
}

TEST_CASE("getFilesToAnalyze_still_matches_file_if_it_was_explicitly_provided")
{
    TempDir t("analyze_cli_ignored_files_explicitly_provided");
    auto fileA = Uri::file(t.write_child("src/a.luau", "")).fsPath();

    CliClient client;
    initCliClient(client);
    client.globalConfig.ignoreGlobs = {"a.luau"};
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    CHECK_EQ(getFilesToAnalyze({fileA}, &workspace), std::vector<std::string>{fileA});
}

TEST_CASE("ignore_globs_from_settings_file_applied")
{
    CliClient client;

    auto configFile = R"({ "luau-lsp.ignoreGlobs": [ "/ignored/**" ] })";

    applySettings(configFile, client);

    REQUIRE_EQ(client.globalConfig.ignoreGlobs.size(), 1);
    CHECK_EQ(client.globalConfig.ignoreGlobs[0], "/ignored/**");
}

TEST_CASE("definition_files_from_settings_file_applied")
{
    CliClient client;

    auto configFile = R"({ "luau-lsp.types.definitionFiles": [ "global_types/types.d.luau" ] })";

    applySettings(configFile, client);

    REQUIRE_EQ(client.definitionsFiles.size(), 1);
    REQUIRE(client.definitionsFiles.find("@roblox1") != client.definitionsFiles.end());
    CHECK_EQ(client.definitionsFiles["@roblox1"], "global_types/types.d.luau");
}

TEST_CASE("enable_new_solver_fflag_from_settings_file_applied")
{
    ScopedFastFlag sff{FFlag::LuauSolverV2, false};

    CliClient client;

    auto configFile = R"({ "luau-lsp.fflags.enableNewSolver": true })";

    applySettings(configFile, client);

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

TEST_CASE("analyze_file_reports_type_errors")
{
    TempDir t("analyze_cli_type_errors");
    auto filePath = t.write_child("test.luau", R"(
        --!strict
        local x: number = "not a number"
    )");

    CliClient client;
    initCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto cr = workspace.checkSimple(filePath, nullptr);
    CHECK(!cr.errors.empty());
}

TEST_CASE("analyze_file_reports_no_errors_for_valid_code")
{
    TempDir t("analyze_cli_no_errors");
    auto filePath = t.write_child("test.luau", R"(
        --!strict
        local x: number = 42
    )");

    CliClient client;
    initCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto cr = workspace.checkSimple(filePath, nullptr);
    CHECK(cr.errors.empty());
}

TEST_CASE("definitions_loaded_through_workspace_via_client")
{
    TempDir t("analyze_cli_definitions");
    auto filePath = t.write_child("test.luau", R"(
        --!strict
        local x: Instance
    )");

    CliClient client;
    initCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto cr = workspace.checkSimple(filePath, nullptr);
    CHECK(cr.errors.empty());
}

TEST_CASE("sourcemap_loaded_through_workspace_configuration")
{
    TempDir t("analyze_cli_sourcemap");

    auto sourcemapContents = R"({
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "children": [
                    {
                        "name": "Module",
                        "className": "ModuleScript",
                        "filePaths": ["src/Module.luau"]
                    }
                ]
            }
        ]
    })";

    t.write_child("sourcemap.json", sourcemapContents);
    t.write_child("src/Module.luau", "return {}");

    CliClient client;
    initRobloxCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    CHECK(robloxPlatform->rootSourceNode != nullptr);
}

TEST_CASE("annotate_retains_type_graphs")
{
    TempDir t("analyze_cli_annotate");
    auto filePath = t.write_child("test.luau", R"(
local function add(a: number, b: number): number
    return a + b
end
    )");

    CliClient client;
    initCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    Luau::ModuleName name = filePath;
    workspace.checkStrict(name, nullptr, /* forAutocomplete= */ false);

    Luau::SourceModule* sm = workspace.frontend.getSourceModule(name);
    Luau::ModulePtr m = workspace.getModule(name);
    REQUIRE(sm);
    REQUIRE(m);

    // attachTypeData requires retained type graphs — would produce empty output
    // if checkSimple (which discards graphs) was used instead
    Luau::attachTypeData(*sm, *m);
    std::string annotated = Luau::prettyPrintWithTypes(*sm->root);
    CHECK(annotated.find("number") != std::string::npos);
}

TEST_CASE("analyze_resolves_game_requires_with_sourcemap")
{
    // Regression test for https://github.com/JohnnyMorganz/luau-lsp/issues/1473
    TempDir t("analyze_game_requires");

    // ServerModule and Util are at completely different filesystem locations but are
    // DataModel siblings — only the sourcemap knows they are related.
    t.write_child("sourcemap.json", R"({
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "children": [
                    {
                        "name": "Util",
                        "className": "ModuleScript",
                        "filePaths": ["packages/util/Util.luau"]
                    }
                ]
            },
            {
                "name": "ServerScriptService",
                "className": "ServerScriptService",
                "children": [
                    {
                        "name": "ServerModule",
                        "className": "ModuleScript",
                        "filePaths": ["src/server/ServerModule.luau"]
                    }
                ]
            }
        ]
    })");
    t.write_child("packages/util/Util.luau", "return { value = 42 }");
    t.write_child("src/server/ServerModule.luau", "local _ = require('@game/ReplicatedStorage/Util')");

    CliClient client;
    initRobloxCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto serverModulePath = Uri::file(t.path() + "/src/server/ServerModule.luau").fsPath();
    std::unordered_set<std::string> reportedModules;
    CHECK(analyzeFile(workspace, serverModulePath, ReportFormat::Default, false, reportedModules));
}

TEST_CASE("analyze_resolves_non_mirrored_relative_requires_with_sourcemap")
{
    // Variant of the above: relative require between DataModel siblings whose
    // filesystem paths are non-mirrored (packages/core/ vs packages/combat/).
    TempDir t("analyze_non_mirrored_relative");

    t.write_child("sourcemap.json", R"({
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "children": [
                    {
                        "name": "ModuleA",
                        "className": "ModuleScript",
                        "filePaths": ["packages/core/ModuleA.luau"]
                    },
                    {
                        "name": "ModuleB",
                        "className": "ModuleScript",
                        "filePaths": ["packages/combat/ModuleB.luau"]
                    }
                ]
            }
        ]
    })");
    t.write_child("packages/core/ModuleA.luau", "local _ = require('./ModuleB')");
    t.write_child("packages/combat/ModuleB.luau", "return {}");

    CliClient client;
    initRobloxCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto moduleAPath = Uri::file(t.path() + "/packages/core/ModuleA.luau").fsPath();
    std::unordered_set<std::string> reportedModules;
    CHECK(analyzeFile(workspace, moduleAPath, ReportFormat::Default, false, reportedModules));
}

TEST_CASE("analyze_resolves_game_requires_from_file_outside_sourcemap")
{
    // `@game` is absolute, so it must resolve from build output that the sourcemap does not cover.
    TempDir t("analyze_game_requires_outside_sourcemap");

    t.write_child("sourcemap.json", R"({
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "children": [
                    {
                        "name": "Util",
                        "className": "ModuleScript",
                        "filePaths": ["packages/util/Util.luau"]
                    }
                ]
            }
        ]
    })");
    t.write_child("packages/util/Util.luau", "return { value = 42 }");
    // dist/ has no sourcemap node.
    t.write_child("dist/main.luau", "local _ = require('@game/ReplicatedStorage/Util')");

    CliClient client;
    initRobloxCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto mainPath = Uri::file(t.path() + "/dist/main.luau").fsPath();
    std::unordered_set<std::string> reportedModules;
    CHECK(analyzeFile(workspace, mainPath, ReportFormat::Default, false, reportedModules));
}

TEST_CASE("analyze_does_not_report_type_errors_of_already_checked_modules_again")
{
    TempDir t("analyze_cli_type_errors_reported_once");
    auto leafPath = t.write_child("leaf.luau", R"(
        --!strict
        local value: number = "not a number"
        return { value = value }
    )");
    auto midPath = t.write_child("mid.luau", R"(
        local leaf = require("./leaf")
        return { value = leaf.value }
    )");
    auto topPath = t.write_child("top.luau", R"(
        local mid = require("./mid")
        return mid.value
    )");

    CliClient client;
    initCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    std::unordered_set<std::string> reportedModules;

    // Checking top.luau also checks mid.luau and leaf.luau, and reports the type error in leaf.luau
    CHECK_FALSE(analyzeFile(workspace, topPath, ReportFormat::Default, false, reportedModules));

    // The error in leaf.luau was already reported, so it is not reported again
    CHECK(analyzeFile(workspace, midPath, ReportFormat::Default, false, reportedModules));
    CHECK(analyzeFile(workspace, leafPath, ReportFormat::Default, false, reportedModules));
}

TEST_CASE("analyze_reports_lints_of_already_checked_modules")
{
    TempDir t("analyze_cli_lints_of_checked_modules");
    t.write_child(".luaurc", R"({ "lintErrors": true })");
    auto leafPath = t.write_child("leaf.luau", R"(
        local unused = 1
        return {}
    )");
    auto topPath = t.write_child("top.luau", R"(
        local _ = require("./leaf")
    )");

    CliClient client;
    initCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    std::unordered_set<std::string> reportedModules;
    CHECK(analyzeFile(workspace, topPath, ReportFormat::Default, false, reportedModules));

    // leaf.luau was checked as a dependency of top.luau, but its lints are only reported when it is analyzed
    CHECK_FALSE(analyzeFile(workspace, leafPath, ReportFormat::Default, false, reportedModules));
}

TEST_CASE("analyze_reports_type_errors_of_a_required_module_after_an_internal_compiler_error")
{
    ScopedFastFlag sff{FFlag::DebugLuauMagicTypes, true};

    TempDir t("analyze_cli_errors_after_internal_compiler_error");
    auto leafPath = t.write_child("leaf.luau", R"(
        --!strict
        local value: number = "not a number"
        return { value = value }
    )");
    // `_luau_ice` throws an InternalCompilerError while checking top.luau, after leaf.luau was checked
    auto topPath = t.write_child("top.luau", R"(
        --!strict
        local leaf = require("./leaf")
        local _crash: _luau_ice = leaf.value
        return leaf
    )");

    CliClient client;
    initCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    std::unordered_set<std::string> reportedModules;
    analyzeFile(workspace, topPath, ReportFormat::Default, false, reportedModules);

    // The error in leaf.luau was not reported when checking top.luau failed, so it is reported now
    CHECK_FALSE(analyzeFile(workspace, leafPath, ReportFormat::Default, false, reportedModules));
}

TEST_SUITE_END();
