#include "doctest.h"
#include "Fixture.h"
#include "LSP/DocumentationParser.hpp"

TEST_SUITE_BEGIN("Hover");

TEST_CASE_FIXTURE(Fixture, "show_string_length_on_hover")
{
    auto source = R"(
        local x = "this is a string"
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 18};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "string (16 bytes)"));
}

TEST_CASE_FIXTURE(Fixture, "show_string_utf8_characters_on_hover")
{
    auto source = R"(
        local x = "this is an emoji: 😁"
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 18};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "string (22 bytes, 19 characters)"));
}

TEST_CASE_FIXTURE(Fixture, "basic_type_alias_declaration")
{
    auto source = R"(
        type Identity = string
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 18};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "type Identity = string"));
}

TEST_CASE_FIXTURE(Fixture, "type_alias_declaration_with_single_generic")
{
    auto source = R"(
        type Identity<T> = T
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 18};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "type Identity<T> = T"));
}

TEST_CASE_FIXTURE(Fixture, "type_alias_declaration_with_generic_default_value")
{
    auto source = R"(
        type Identity<T = string> = T
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 18};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "type Identity<T = string> = T"));
}

TEST_CASE_FIXTURE(Fixture, "type_alias_declaration_with_multiple_generics")
{
    auto source = R"(
        type Identity<T, U = string> = T
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 18};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "type Identity<T, U = string> = T"));
}

TEST_CASE_FIXTURE(Fixture, "type_alias_declaration_generic_type_pack")
{
    auto source = R"(
        type Identity<T...> = (any) -> T...
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 18};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "type Identity<T...> = (any) -> (T...)"));
}

TEST_CASE_FIXTURE(Fixture, "type_alias_declaration_generic_type_pack_with_default")
{
    auto source = R"(
        type Identity<T... = ...string> = (any) -> T...
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 18};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "type Identity<T... = ...string> = (any) -> (T...)"));
}

TEST_CASE_FIXTURE(Fixture, "complex_type_alias_declaration_with_generics")
{
    auto source = R"(
        type Identity<T, U = number, V... = ...string> = (T, U) -> V...
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 18};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "type Identity<T, U = number, V... = ...string> = (T, U) -> (V...)"));
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_for_a_type_table")
{
    auto source = R"(
        --- This is documentation for Foo
        type Foo = {
        }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{2, 14};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "type Foo = {  }") + kDocumentationBreaker + "This is documentation for Foo\n");
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_for_a_type_table_when_hovering_over_variable_with_type")
{
    auto source = R"(
        --- This is documentation for Foo
        type Foo = {
        }
        local x: Foo = nil
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{4, 14};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "local x: {  }") + kDocumentationBreaker + "This is documentation for Foo\n");
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_for_a_member_of_a_type_table")
{
    auto source = R"(
        --- This is documentation for Foo
        type Foo = {
            --- This is a member bar
            bar: string,
        }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{4, 13};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "string") + kDocumentationBreaker + "This is a member bar\n");
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_for_a_member_of_a_type_table_when_hovering_over_property")
{
    auto source = R"(
        --- This is documentation for Foo
        type Foo = {
            --- This is a member bar
            bar: string,
        }
        local x: Foo
        local y = x.bar
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{7, 21};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "string") + kDocumentationBreaker + "This is a member bar\n");
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_for_a_member_of_an_intersected_type_table_when_hovering_over_property")
{
    auto source = R"(
        type A = {
            --- Example sick number
            Hello: number
        }

        type B = {
            --- Example sick string
            Heya: string
        } & A

        local item: B = nil
        print(item.Heya)
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{12, 21};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "string") + kDocumentationBreaker + "Example sick string\n");
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_for_a_function")
{
    auto source = R"(
        --- This is documentation for Foo
        function foo()
        end
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{2, 18};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "function foo(): ()") + kDocumentationBreaker + "This is documentation for Foo\n");
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_for_a_function_call")
{
    auto source = R"(
        --- This is documentation for Foo
        function foo()
        end
        foo()
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{4, 9};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "function foo(): ()") + kDocumentationBreaker + "This is documentation for Foo\n");
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_for_type_alias_declarations")
{
    auto source = R"(
        --- The metre (or meter in [US spelling]; symbol: m) is the [base unit] of [length]
        --- in the [International System of Units] (SI)
        export type Meters = number
    )";

    auto uri = newDocument("meters.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{3, 21};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(
        result->contents.value, 
        codeBlock("luau", "type Meters = number") + 
        kDocumentationBreaker + 
        "The metre (or meter in [US spelling]; symbol: m) is the [base unit] of [length]\n" + 
        "in the [International System of Units] (SI)\n"
    );
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_for_type_alias_declarations_of_intersected_tables")
{
    auto source = R"(
        type Foo = {
            foo: "Foo",
        }

        type Bar = {
            bar: "Bar",
        }

        --- The terms foobar (/ˈfuːbɑːr/), foo, bar, baz, qux, quux, and others are used as
        --- metasyntactic variables and placeholder names in computer programming or computer-related documentation
        export type Foobar = Foo & Bar
    )";

    auto uri = newDocument("meters.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{11, 21};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(
        result->contents.value, 
        codeBlock("luau", "type Foobar = {\n    bar: \"Bar\"\n} & {\n    foo: \"Foo\"\n}") + 
        kDocumentationBreaker + 
        "The terms foobar (/ˈfuːbɑːr/), foo, bar, baz, qux, quux, and others are used as\n" +
        "metasyntactic variables and placeholder names in computer programming or computer-related documentation\n"
    );
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_for_type_references")
{
    auto source = R"(
        type Foo = {
            foo: "Foo",
        }

        type Bar = {
            bar: "Bar",
        }

        --- This is the intersection of two types
        export type Foobar = Foo & Bar

        function consumer(value: Foobar)
        end
    )";

    auto uri = newDocument("meters.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{12, 36};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "type Foobar = {\n    bar: \"Bar\"\n} & {\n    foo: \"Foo\"\n}") + kDocumentationBreaker +
                                         "This is the intersection of two types\n");
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_for_external_type_references")
{
    auto source = newDocument("types.luau", R"(
        --- This is a type
        export type Value = string
    )");

    auto uri = newDocument("source.luau", R"(
        local Types = require("types.luau")

        local x: Types.Value
    )");

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{3, 25};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "type Types.Value = string") + kDocumentationBreaker + "This is a type\n");
}

TEST_CASE_FIXTURE(Fixture, "handles_type_references_without_types_graph")
{
    auto source = newDocument("types.luau", R"(
        --- This is a type
        export type Value = string
    )");

    auto uri = newDocument("source.luau", R"(
        local Types = require("types.luau")

        local x: Types.Value
    )");

    // This test explicitly expects type graphs to not be retained (i.e., the required module scope was cleared)
    // We should still be able to find the type references.
    workspace.checkSimple(workspace.fileResolver.getModuleName(uri), /* cancellationToken= */ nullptr);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{3, 25};

    auto result = workspace.hover(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "type Types.Value = string") + kDocumentationBreaker + "This is a type\n");
}

TEST_CASE_FIXTURE(Fixture, "hover_respects_cancellation")
{
    auto cancellationToken = std::make_shared<Luau::FrontendCancellationToken>();
    cancellationToken->cancel();

    auto document = newDocument("a.luau", "local x = 1");
    CHECK_THROWS_AS(workspace.hover(lsp::HoverParams{{{document}}}, cancellationToken), RequestCancelledException);
}

TEST_SUITE_END();
