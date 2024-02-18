#include "doctest.h"
#include "LSP/Sourcemap.hpp"

TEST_SUITE_BEGIN("SourcemapTests");

TEST_CASE("getScriptFilePath")
{
    SourceNode node;
    node.className = "ModuleScript";
    node.filePaths = {"test.lua"};
    CHECK_EQ(node.getScriptFilePath(), "test.lua");
}

TEST_CASE("getScriptFilePath returns json file if node is populated by JSON")
{
    SourceNode node;
    node.className = "ModuleScript";
    node.filePaths = {"test.json"};
    CHECK_EQ(node.getScriptFilePath(), "test.json");
}

TEST_CASE("getScriptFilePath returns toml file if node is populated by TOML")
{
    SourceNode node;
    node.className = "ModuleScript";
    node.filePaths = {"test.toml"};
    CHECK_EQ(node.getScriptFilePath(), "test.toml");
}

TEST_CASE("getScriptFilePath doesn't pick .meta.json")
{
    SourceNode node;
    node.className = "ModuleScript";
    node.filePaths = {"init.meta.json", "init.lua"};
    CHECK_EQ(node.getScriptFilePath(), "init.lua");
}

TEST_SUITE_END();