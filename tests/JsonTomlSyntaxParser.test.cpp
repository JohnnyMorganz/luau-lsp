#include "doctest.h"

#include "LSP/JsonTomlSyntaxParser.hpp"
#include "Luau/Parser.h"
#include "Luau/PrettyPrinter.h"
#include "Fixture.h"

using namespace toml::literals::toml_literals;

TEST_SUITE_BEGIN("JsonTomlSyntaxParser");

TEST_CASE_FIXTURE(Fixture, "jsonValueToLuau returns proper Luau string")
{
    nlohmann::json json = {
        {"int", 123},
        {"float", 123.456},
        {"string", "hello"},
        {"bool", true},
        {"array", {1, 2, 3}},
        {"object", {{"key", "value"}}},
    };

    auto block = parse("return " + jsonValueToLuau(json));
    REQUIRE(block->body.size == 1);

    auto returnStmt = (*block->body.begin())->as<Luau::AstStatReturn>();
    REQUIRE(returnStmt);
    REQUIRE(returnStmt->list.size == 1);

    auto table = (*returnStmt->list.begin())->as<Luau::AstExprTable>();
    REQUIRE(table);
    CHECK(table->items.size == 6);
}

TEST_CASE_FIXTURE(Fixture, "jsonValueToLuau escapes strings")
{
    nlohmann::json json = {
        {"newLineKey", "a\nb"},
        {"a\nb", "newLineValue"},
        {"quoteKey", "a\"b"},
        {"a\"b", "quoteValue"},
    };

    auto block = parse("return " + jsonValueToLuau(json));
    REQUIRE(block->body.size == 1);

    auto returnStmt = (*block->body.begin())->as<Luau::AstStatReturn>();
    REQUIRE(returnStmt);
    REQUIRE(returnStmt->list.size == 1);

    auto table = (*returnStmt->list.begin())->as<Luau::AstExprTable>();
    REQUIRE(table);

    // Verify all 4 items parsed correctly (keys and values escaped)
    CHECK(table->items.size == 4);
}

static Luau::AstExprTable* parseLuauTable(const Luau::AstStatBlock* block)
{
    REQUIRE(block->body.size == 1);

    auto returnStmt = (*block->body.begin())->as<Luau::AstStatReturn>();
    REQUIRE(returnStmt);
    REQUIRE(returnStmt->list.size == 1);

    return (*returnStmt->list.begin())->as<Luau::AstExprTable>();
}

static void expectItem(const Luau::AstExprTable* table, const std::string& key, const std::string& value)
{
    bool found = false;

    for (const auto& item : table->items)
    {
        REQUIRE(item.kind == Luau::AstExprTable::Item::Kind::General);
        auto itemKey = item.key->as<Luau::AstExprConstantString>();
        REQUIRE(itemKey);

        if (std::string(itemKey->value.data, itemKey->value.size) == key)
        {
            CHECK_EQ(Luau::toString(item.value), value);
            found = true;
        }
    }
    CHECK_MESSAGE(found == true, key);
}

TEST_CASE_FIXTURE(Fixture, "tomlValueToLuau returns proper Luau string")
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

    auto block = parse("return " + tomlValueToLuau(toml));
    auto table = parseLuauTable(block);

    expectItem(table, "int", "123");
    expectItem(table, "float", "123.456");
    expectItem(table, "string", "'hello'");
    expectItem(table, "bool", "true");
    expectItem(table, "array", "{1,2,3 }");
    expectItem(table, "object", "{['key'] = 'value' }");
    expectItem(table, "nested", "{['values'] = {1,2,3 } }");
}

TEST_CASE_FIXTURE(Fixture, "tomlValueToLuau escapes strings")
{
    toml::value toml = R"(
        newLineKey = "a\nb"
        "a\nb" = "newLineValue"

        quoteKey = "a\"b"
        "a\"b" = "quoteValue"
    )"_toml;

    auto block = parse("return " + tomlValueToLuau(toml));
    auto table = parseLuauTable(block);

    expectItem(table, "newLineKey", "'a\\nb'");
    expectItem(table, "a\nb", "'newLineValue'");
    expectItem(table, "quoteKey", "'a\\\"b'");
    expectItem(table, "a\"b", "'quoteValue'");
}

TEST_CASE_FIXTURE(Fixture, "yamlValueToLuau returns proper Luau string")
{
    const char yaml_str[] = R"(
string: this is a string
boolean: true
integer: 1337
float: 123456789.5
value-with-hypen: it sure is
sequence:
  - wow
  - 8675309
map:
  key: value
  key2: "value 2"
  key3: 'value 3'
nested-map:
  - key: value
  - key2: "value 2"
  - key3: "value 3"
whatever_this_is: [i imagine, it's, a, sequence?]
null1: ~
null2: null
)";

    ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(yaml_str));
    auto block = parse("return " + yamlValueToLuau(tree.rootref()));
    auto table = parseLuauTable(block);

    expectItem(table, "string", "'this is a string'");
    expectItem(table, "boolean", "true");
    expectItem(table, "integer", "1337");
    expectItem(table, "value-with-hypen", "'it sure is'");
    expectItem(table, "null1", "nil");
    expectItem(table, "null2", "nil");
}

TEST_CASE_FIXTURE(Fixture, "yamlValueToLuau handles null values")
{
    const char yaml_str[] = R"(
null1: ~
null2: null
null3: Null
null4: NULL
)";

    ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(yaml_str));
    auto block = parse("return " + yamlValueToLuau(tree.rootref()));
    auto table = parseLuauTable(block);

    expectItem(table, "null1", "nil");
    expectItem(table, "null2", "nil");
    expectItem(table, "null3", "nil");
    expectItem(table, "null4", "nil");
}

TEST_CASE_FIXTURE(Fixture, "yamlValueToLuau escapes strings")
{
    const char yaml_str[] = R"(
newLineKey: "a\nb"
quoteKey: 'a"b'
)";

    ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(yaml_str));
    auto block = parse("return " + yamlValueToLuau(tree.rootref()));
    auto table = parseLuauTable(block);

    expectItem(table, "newLineKey", "'a\\nb'");
    expectItem(table, "quoteKey", "'a\\\"b'");
}

TEST_CASE_FIXTURE(Fixture, "yamlValueToLuau handles keys with empty values")
{
    const char yaml_str[] = R"(experience_id: 12345
passes:

products:
)";

    ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(yaml_str));
    auto result = yamlValueToLuau(tree.rootref());

    auto block = parse("return " + result);
    auto table = parseLuauTable(block);

    REQUIRE(table);
    expectItem(table, "experience_id", "12345");
    expectItem(table, "passes", "nil");
    expectItem(table, "products", "nil");
}

TEST_CASE("jsoncToJson removes line and block comments")
{
    auto result = nlohmann::json::parse(jsoncToJson(R"(
    {
        // line comment
        "a": 1, /* block
        comment */ "b": [1, /* inline */ 2]
    } // trailing comment
    )"));

    CHECK_EQ(result, nlohmann::json::parse(R"({"a": 1, "b": [1, 2]})"));
}

TEST_CASE("jsoncToJson removes trailing commas")
{
    auto result = nlohmann::json::parse(jsoncToJson(R"(
    {
        "a": [1, 2,],
        "b": {"c": 3,},
    }
    )"));

    CHECK_EQ(result, nlohmann::json::parse(R"({"a": [1, 2], "b": {"c": 3}})"));
}

TEST_CASE("jsoncToJson removes trailing commas separated from the closing bracket by comments")
{
    auto result = nlohmann::json::parse(jsoncToJson("[1, 2, // two\n /* end */ ]"));
    CHECK_EQ(result, nlohmann::json::parse("[1, 2]"));
}

TEST_CASE("jsoncToJson leaves string contents untouched")
{
    auto result = nlohmann::json::parse(jsoncToJson(R"({"url": "http://example.com", "text": "/* not a comment */, ]", "quote": "a\"//b",})"));

    CHECK_EQ(result["url"], "http://example.com");
    CHECK_EQ(result["text"], "/* not a comment */, ]");
    CHECK_EQ(result["quote"], "a\"//b");
}

TEST_CASE("jsoncToJson leaves plain JSON unchanged")
{
    const std::string json = R"({"a": [1, 2, {"b": null}], "c": "d"})";
    CHECK_EQ(jsoncToJson(json), json);
}

TEST_CASE("jsoncToJson does not make invalid JSON valid")
{
    auto isValid = [](std::string_view jsonc)
    {
        return !nlohmann::json::parse(jsoncToJson(jsonc), nullptr, false).is_discarded();
    };

    CHECK_FALSE(isValid("[,]"));
    CHECK_FALSE(isValid("[1,,]"));
    CHECK_FALSE(isValid("{,}"));
    CHECK_FALSE(isValid("[1, 2] /* unterminated"));
    CHECK_FALSE(isValid(R"(["unterminated])"));
    CHECK(isValid("[1, 2,]"));
}

TEST_SUITE_END();
