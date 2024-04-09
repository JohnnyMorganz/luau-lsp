#include "doctest.h"
#include "LSP/Utils.hpp"
#include "Platform/RobloxPlatform.hpp"

TEST_SUITE_BEGIN("UtilsTest");

TEST_CASE("getAncestorPath finds ancestor from given name")
{
    CHECK_EQ(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "Module", nullptr), "game/ReplicatedStorage/Module");
}

TEST_CASE("getAncestorPath handles when ancestor is not found")
{
    CHECK_FALSE(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "NonExistent", nullptr).has_value());
}

TEST_CASE("getAncestorPath handles when ancestor is root of DataModel node")
{
    CHECK_EQ(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "game", nullptr), "game");
}

TEST_CASE("getAncestorPath handles when ancestor is root of non-DataModel node")
{
    SourceNode node;
    node.name = "Foo";

    CHECK_EQ(getAncestorPath("ProjectRoot/Bar", "Foo", std::make_shared<SourceNode>(node)), "ProjectRoot");
}

TEST_CASE("getAncestorPath handles when ancestor is root of non-DataModel node and its name has multiple occurrences")
{
    SourceNode node;
    node.name = "Foo";

    CHECK_EQ(getAncestorPath("ProjectRoot/Bar/Foo/Baz", "Foo", std::make_shared<SourceNode>(node)), "ProjectRoot/Bar/Foo");
}

TEST_CASE("getAncestorPath returns nothing when ancestorName == current name, and no ancestor of name found")
{
    CHECK_FALSE(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "Foo", nullptr).has_value());
}

TEST_CASE("getAncestorPath handles when ancestor name is the same as current name")
{
    CHECK_EQ(getAncestorPath("game/ReplicatedStorage/Module/Child/Module", "Module", nullptr), "game/ReplicatedStorage/Module");
}

TEST_CASE("convertToScriptPath handles when path is empty")
{
    CHECK_EQ(convertToScriptPath(""), "");
}

TEST_CASE("convertToScriptPath handles when path is root")
{
    CHECK_EQ(convertToScriptPath("game"), "game");
}

TEST_CASE("convertToScriptPath handles when path is a single folder")
{
    CHECK_EQ(convertToScriptPath("game/ReplicatedStorage"), "game.ReplicatedStorage");
}

TEST_CASE("convertToScriptPath handles whitespaces in path")
{
    CHECK_EQ(convertToScriptPath("game/Replicated Storage/Common"), "game[\"Replicated Storage\"].Common");
}

TEST_CASE("convertToScriptPath handles relative paths")
{
    CHECK_EQ(convertToScriptPath("../Child.Foo"), "script.Parent.Child.Foo");
}

TEST_CASE("getHomeDirectory finds a home directory")
{
    // we cannot confirm *what* the home directory is
    // since it varies per test runner
    CHECK(getHomeDirectory());
}

TEST_CASE("resolvePath resolves paths including tilde expansions")
{
    CHECK_EQ(resolvePath("C:/Users/test/foo.lua"), "C:/Users/test/foo.lua");

    auto home = getHomeDirectory();
    REQUIRE(home);

    CHECK_EQ(resolvePath("~/foo.lua"), home.value() / "foo.lua");
}

TEST_CASE("isDataModel returns true when path starts with game")
{
    CHECK_EQ(isDataModel("game/ReplicatedStorage"), true);
}

TEST_CASE("isDataModel returns false when path starts with ProjectRoot")
{
    CHECK_EQ(isDataModel("ProjectRoot/Foo/Bar"), false);
}

TEST_CASE("getFirstLine returns first line")
{
    CHECK_EQ(getFirstLine("--##{'x':true}\nhello = world"), "--##{'x':true}");
}

TEST_CASE("getFirstLine returns string when there is no newline")
{
    CHECK_EQ(getFirstLine(""), "");
    CHECK_EQ(getFirstLine("testing"), "testing");
}

TEST_SUITE_END();
