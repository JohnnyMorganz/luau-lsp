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

TEST_CASE("ignore_globs_from_settings_file_applied")
{
    CliClient client;
    std::vector<std::string> ignoreGlobs;
    std::vector<std::filesystem::path> definitionPaths;

    auto configFile = R"({ "luau-lsp.ignoreGlobs": [ "/ignored/**" ] })";

    applySettings(configFile, client, ignoreGlobs, definitionPaths);

    REQUIRE_EQ(ignoreGlobs.size(), 1);
    CHECK_EQ(ignoreGlobs[0], "/ignored/**");
}

TEST_CASE("definition_files_from_settings_file_applied")
{
    CliClient client;
    std::vector<std::string> ignoreGlobs;
    std::vector<std::filesystem::path> definitionPaths;

    auto configFile = R"({ "luau-lsp.types.definitionFiles": [ "global_types/types.d.luau" ] })";

    applySettings(configFile, client, ignoreGlobs, definitionPaths);

    REQUIRE_EQ(definitionPaths.size(), 1);
    CHECK_EQ(definitionPaths[0], "global_types/types.d.luau");
}

TEST_SUITE_END();
