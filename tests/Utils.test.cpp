#include "doctest.h"
#include "LSP/Utils.hpp"
#include "LSP/Sourcemap.hpp"

TEST_SUITE_BEGIN("UtilsTest");

TEST_CASE("getAncestorPath finds ancestor from given name")
{
    auto sourceNode = std::make_shared<SourceNode>(SourceNode{});

    CHECK_EQ(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "Module", sourceNode), "game/ReplicatedStorage/Module");
}

TEST_CASE("getAncestorPath handles when ancestor is not found")
{
    auto sourceNode = std::make_shared<SourceNode>(SourceNode{});

    CHECK_FALSE(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "NonExistent", sourceNode).has_value());
}

TEST_CASE("getAncestorPath handles when ancestor is root of DataModel node")
{
    auto sourceNode = std::make_shared<SourceNode>(SourceNode{});

    CHECK_EQ(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "game", sourceNode), "game");
}

TEST_CASE("getAncestorPath handles when ancestor is root of non-DataModel node")
{
    auto sourceNode = std::make_shared<SourceNode>(SourceNode{
        .name = "Foo",
    });

    CHECK_EQ(getAncestorPath("ProjectRoot/Bar", "Foo", sourceNode), "ProjectRoot");
}

TEST_CASE("getAncestorPath returns nothing when ancestorName == current name, and no ancestor of name found")
{
    auto sourceNode = std::make_shared<SourceNode>(SourceNode{});

    CHECK_FALSE(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "Foo", sourceNode).has_value());
}

TEST_CASE("getAncestorPath handles when ancestor name is the same as current name")
{
    auto sourceNode = std::make_shared<SourceNode>(SourceNode{});

    CHECK_EQ(getAncestorPath("game/ReplicatedStorage/Module/Child/Module", "Module", sourceNode), "game/ReplicatedStorage/Module");
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