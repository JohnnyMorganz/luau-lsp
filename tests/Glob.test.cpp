#include "doctest.h"
#include "glob/match.h"

TEST_SUITE_BEGIN("Glob");

TEST_CASE("matches_exact_file_name")
{
    auto PATTERN = "foo.luau";

    CHECK_EQ(glob::gitignore_glob_match("/home/project", PATTERN), false);
    CHECK_EQ(glob::gitignore_glob_match("/home/project/random.luau", PATTERN), false);
    CHECK_EQ(glob::gitignore_glob_match("/home/project/foo.luau", PATTERN), true);

    CHECK_EQ(glob::gitignore_glob_match("random.luau", PATTERN), false);
    CHECK_EQ(glob::gitignore_glob_match("foo.luau", PATTERN), true);
}

TEST_CASE("matches_file_ending")
{
    auto PATTERN = "*.luau";

    CHECK_EQ(glob::gitignore_glob_match("/home/project", PATTERN), false);
    CHECK_EQ(glob::gitignore_glob_match("/home/project/foo.lua", PATTERN), false);
    CHECK_EQ(glob::gitignore_glob_match("/home/project/foo.luau", PATTERN), true);

    CHECK_EQ(glob::gitignore_glob_match("foo.lua", PATTERN), false);
    CHECK_EQ(glob::gitignore_glob_match("foo.luau", PATTERN), true);
}

TEST_CASE("matches_default_wally_index_glob")
{
    auto PATTERN = "**/_Index/**";

    CHECK_EQ(glob::gitignore_glob_match("/home/project", PATTERN), false);
    CHECK_EQ(glob::gitignore_glob_match("/home/project/Packages", PATTERN), false);
    CHECK_EQ(glob::gitignore_glob_match("/home/project/Packages/_Index", PATTERN), false); // TODO: should this be true?
    CHECK_EQ(glob::gitignore_glob_match("/home/project/Packages/_Index/Project", PATTERN), true);
    CHECK_EQ(glob::gitignore_glob_match("/home/project/Packages/_Index/Project/init.luau", PATTERN), true);

    CHECK_EQ(glob::gitignore_glob_match("Packages/_Index", PATTERN), false); // TODO: should this be true?
    CHECK_EQ(glob::gitignore_glob_match("Packages/_Index/Project", PATTERN), true);
    CHECK_EQ(glob::gitignore_glob_match("Packages/_Index/Project/init.luau", PATTERN), true);

    CHECK_EQ(glob::gitignore_glob_match("_Index/", PATTERN), true);
    CHECK_EQ(glob::gitignore_glob_match("_Index/Project/", PATTERN), true);
    CHECK_EQ(glob::gitignore_glob_match("_Index/Project/init.luau", PATTERN), true);
}

TEST_SUITE_END();
