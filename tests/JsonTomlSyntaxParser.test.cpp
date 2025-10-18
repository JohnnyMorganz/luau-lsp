#include "doctest.h"

#include "LSP/JsonTomlSyntaxParser.hpp"
#include "Luau/Parser.h"
#include "Luau/PrettyPrinter.h"
#include "Fixture.h"

using namespace toml::literals::toml_literals;

TEST_SUITE_BEGIN("JsonTomlSyntaxParser");

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

TEST_SUITE_END();
