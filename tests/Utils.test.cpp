#include "doctest.h"
#include "LSP/Utils.hpp"

TEST_SUITE_BEGIN("UtilsTest");

TEST_CASE("getAncestorPath finds ancestor from given name")
{
    CHECK_EQ(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "Module"), "game/ReplicatedStorage/Module");
};

TEST_CASE("getAncestorPath handles when ancestor is not found")
{
    CHECK_FALSE(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "NonExistent").has_value());
};

TEST_CASE("getAncestorPath handles when ancestor is root")
{
    CHECK_EQ(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "game"), "game");
};

TEST_CASE("getAncestorPath returns nothing when ancestorName == current name, and no ancestor of name found")
{
    CHECK_FALSE(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "Foo").has_value());
};

TEST_CASE("getAncestorPath handles when ancestor name is the same as current name")
{
    CHECK_EQ(getAncestorPath("game/ReplicatedStorage/Module/Child/Module", "Module"), "game/ReplicatedStorage/Module");
};

TEST_CASE("pathDistance returns 0 when paths are the same")
{
    CHECK_EQ(pathDistance("game/ReplicatedStorage/Module/Child/Foo", "game/ReplicatedStorage/Module/Child/Foo"), 0);
};

TEST_CASE("pathDistance returns 1 when paths are adjacent")
{
    CHECK_EQ(pathDistance("game/ReplicatedStorage/Module/Child/Foo", "game/ReplicatedStorage/Module/Child/Bar"), 1);
};

TEST_CASE("pathDistance returns 2 when paths are 2 apart")
{
    CHECK_EQ(pathDistance("game/ReplicatedStorage/Module/Child/Foo", "game/ReplicatedStorage/Module/Bar"), 2);
};

TEST_CASE("convertToScriptPath handles when path is empty")
{
    CHECK_EQ(convertToScriptPath(""), "");
};

TEST_CASE("convertToScriptPath handles when path is root")
{
    CHECK_EQ(convertToScriptPath("game"), "game");
};

TEST_CASE("convertToScriptPath handles when path is a single folder")
{
    CHECK_EQ(convertToScriptPath("game/ReplicatedStorage"), "game.ReplicatedStorage");
};

TEST_CASE("convertToScriptPath handles whitespaces in path")
{
    CHECK_EQ(convertToScriptPath("game/Replicated Storage/Common"), "game[\"Replicated Storage\"].Common");
};

TEST_CASE("convertToScriptPath handles relative paths")
{
    CHECK_EQ(convertToScriptPath("../Child.Foo"), "script.Parent.Child.Foo");
};

TEST_SUITE_END();