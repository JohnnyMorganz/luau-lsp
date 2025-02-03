#include "doctest.h"
#include "TempDir.h"
#include "Analyze/AnalyzeCli.hpp"
#include "Analyze/CliConfigurationParser.hpp"
#include "LSP/Utils.hpp"

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
    auto fileA = t.write_child("src/a.luau", "");
    auto fileB = t.write_child("src/b.luau", "");

    auto allResults = getFilesToAnalyze({t.path()}, {});
    std::sort(allResults.begin(), allResults.end());
    CHECK_EQ(allResults, std::vector<std::filesystem::path>{fileA, fileB});

    auto ignoredFile = getFilesToAnalyze({t.path()}, {"b.luau"});
    std::sort(ignoredFile.begin(), ignoredFile.end());
    CHECK_EQ(ignoredFile, std::vector<std::filesystem::path>{fileA});
}

TEST_CASE("getFilesToAnalyze_handles_ignore_globs_within_directories")
{
    TempDir t("analyze_cli_ignored_files");
    auto fileA = t.write_child("src/a.luau", "");
    auto fileB = t.write_child("src/b.luau", "");

    auto ignoredFile = getFilesToAnalyze({t.path()}, {"b.luau"});
    std::sort(ignoredFile.begin(), ignoredFile.end());
    CHECK_EQ(ignoredFile, std::vector<std::filesystem::path>{fileA});
}

TEST_CASE("getFilesToAnalyze_still_matches_file_if_it_was_explicitly_provided")
{
    TempDir t("analyze_cli_ignored_files_explicitly_provided");
    auto fileA = t.write_child("src/a.luau", "");
    CHECK_EQ(getFilesToAnalyze({fileA}, {"a.luau"}), std::vector<std::filesystem::path>{fileA});
}

TEST_CASE("getFilesToAnalyze_handles_settings_file")
{
    TempDir t("analyze_cli_handles_settings_file");
    auto configFile = t.write_child(".vscode/settings.json", "{ \"luau-lsp.ignoreGlobs\": [ \"/ignored/**\" ] }");
    t.write_child("ignored/ignore.luau", "invalid luau code function do end");
    auto fileA = t.write_child("src/init.luau", "print(require(\"./ignored/ignore\"))");

    auto configContent = readFile(configFile);
    REQUIRE(configContent.has_value());

    auto config = dottedToClientConfiguration(configContent.value());
    CHECK_EQ(config.ignoreGlobs, std::vector<std::string>{"/ignored/**"});
    auto files = getFilesToAnalyze({fileA}, config.ignoreGlobs);
    CHECK_EQ(files, std::vector<std::filesystem::path>{fileA});
}

TEST_CASE("getFilesToAnalyze_def_files_settings_file")
{
    TempDir t("analyze_cli_def_files_settings_file");
    auto configFile = t.write_child(".vscode/settings.json", "{ \"luau-lsp.types.definitionFiles\": [ \"global_types/types.d.luau\" ] }");
    t.write_child("global_types/types.d.luau", "declare TEST: boolean");
    t.write_child("src/init.luau", "print(TEST)");

    auto configContent = readFile(configFile);
    REQUIRE(configContent.has_value());

    auto config = dottedToClientConfiguration(configContent.value());
    CHECK_EQ(config.types.definitionFiles, std::vector<std::filesystem::path>{"global_types/types.d.luau"});
}

TEST_SUITE_END();
