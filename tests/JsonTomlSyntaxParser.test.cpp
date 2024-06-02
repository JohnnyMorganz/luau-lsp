#include "doctest.h"

#include "LSP/JsonTomlSyntaxParser.hpp"

using namespace toml::literals::toml_literals;

TEST_SUITE_BEGIN("JsonTomlSyntaxParser");

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
