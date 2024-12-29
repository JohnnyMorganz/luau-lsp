#include "doctest.h"
#include "TempDir.h"
#include "Analyze/AnalyzeCli.hpp"

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

    CHECK_EQ(definitionsFiles, std::unordered_map<std::string, std::filesystem::path>{
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

    CHECK_EQ(definitionsFiles, std::unordered_map<std::string, std::filesystem::path>{
                                   {"@roblox", "example_path.d.luau"},
                                   {"@roblox1", "lune.d.luau"},
                               });
}

TEST_SUITE_END();
