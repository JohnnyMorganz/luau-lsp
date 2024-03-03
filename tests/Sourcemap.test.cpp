#include "doctest.h"
#include "LSP/Sourcemap.hpp"
using namespace toml::literals::toml_literals;

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

TEST_CASE("tomlValueToLuau returns proper Luau string")
{
    toml::value toml = R"(
        int = 123
        float = 123.456
        string = "hello"
        bool = true
        array = [1, 2, 3]
        object = { key = "value" }

        [nested]
        values = [1, 2, 3]
    )"_toml;

    std::string luau =
        R"({["nested"] = {["values"] = {1;2;3;};};["object"] = {["key"] = "value";};["array"] = {1;2;3;};["float"] = 123.456;["bool"] = true;["string"] = "hello";["int"] = 123;})";

    CHECK_EQ(tomlValueToLuau(toml), luau);
}

TEST_CASE("tomlValueToLuau escapes strings")
{
    toml::value toml = R"(
        newLineKey = "a\nb"
        "a\nb" = "newLineValue"

        quoteKey = "a\"b"
        "a\"b" = "quoteValue"
    )"_toml;

    std::string luau = R"({["quoteKey"] = "a\"b";["a\"b"] = "quoteValue";["newLineKey"] = "a\nb";["a\nb"] = "newLineValue";})";

    CHECK_EQ(tomlValueToLuau(toml), luau);
}

TEST_SUITE_END();
