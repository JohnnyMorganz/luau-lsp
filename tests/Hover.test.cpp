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

    auto result = workspace.hover(params);
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

    auto result = workspace.hover(params);
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

    auto result = workspace.hover(params);
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

    auto result = workspace.hover(params);
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

    auto result = workspace.hover(params);
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

    auto result = workspace.hover(params);
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

    auto result = workspace.hover(params);
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

    auto result = workspace.hover(params);
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

    auto result = workspace.hover(params);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "type Identity<T, U = number, V... = ...string> = (T, U) -> (V...)"));
}

TEST_CASE_FIXTURE(Fixture, "show_type_of_global_variable")
{
    auto source = R"(
        print(DocumentedGlobalVariable)
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 23};

    auto result = workspace.hover(params);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "type DocumentedGlobalVariable = number"));
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

    auto result = workspace.hover(params);
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
    params.position = lsp::Position{5, 14};

    auto result = workspace.hover(params);
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

    auto result = workspace.hover(params);
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
    params.position = lsp::Position{8, 21};

    auto result = workspace.hover(params);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "string") + kDocumentationBreaker + "This is a member bar\n");
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_for_a_global_type_table_from_definitions_file")
{
    auto source = R"(
        local x: DocumentedTable = nil
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 24};

    auto result = workspace.hover(params);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "type DocumentedTable = {\n"
                                                       "    member1: string\n"
                                                       "}") +
                                         kDocumentationBreaker + "This is a documented table\n");
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_for_a_global_type_table_from_definitions_file_when_hovering_over_variable_with_type")
{
    auto source = R"(
        local x: DocumentedTable = nil
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 14};

    auto result = workspace.hover(params);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "local x: {\n"
                                                       "    member1: string\n"
                                                       "}") +
                                         kDocumentationBreaker + "This is a documented table\n");
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_for_a_global_type_table_from_definitions_file_when_hovering_over_property")
{
    auto source = R"(
        local x: DocumentedTable = nil
        local y = x.member1
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{2, 23};

    auto result = workspace.hover(params);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "string") + kDocumentationBreaker + "This is documented member1 of the table\n");
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

    auto result = workspace.hover(params);
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
    params.position = lsp::Position{5, 9};

    auto result = workspace.hover(params);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "function foo(): ()") + kDocumentationBreaker + "This is documentation for Foo\n");
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_for_a_global_function_call_from_definitions_file")
{
    auto source = R"(
        DocumentedGlobalFunction()
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 20};

    auto result = workspace.hover(params);
    REQUIRE(result);
    CHECK_EQ(result->contents.value,
        codeBlock("luau", "function DocumentedGlobalFunction(): number") + kDocumentationBreaker + "This is a documented global function\n");
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_when_hovering_over_class_type_from_definitions_file")
{
    auto source = R"(
        local x: DocumentedClass
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 23};

    auto result = workspace.hover(params);
    REQUIRE(result);
    CHECK_EQ(
        result->contents.value, codeBlock("luau", "type DocumentedClass = DocumentedClass") + kDocumentationBreaker + "This is a documented class\n");
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_when_hovering_over_variable_with_class_type")
{
    auto source = R"(
        local x: DocumentedClass
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 14};

    auto result = workspace.hover(params);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "local x: DocumentedClass") + kDocumentationBreaker + "This is a documented class\n");
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_when_hovering_over_class_type_property")
{
    auto source = R"(
        local x: DocumentedClass
        local y = x.member1
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{2, 23};

    auto result = workspace.hover(params);
    REQUIRE(result);
    CHECK_EQ(result->contents.value, codeBlock("luau", "string") + kDocumentationBreaker + "This is a documented member1 of the class\n");
}

TEST_CASE_FIXTURE(Fixture, "includes_documentation_when_hovering_over_class_type_method_call")
{
    auto source = R"(
        local x: DocumentedClass
        local y = x:function1()
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{2, 23};

    auto result = workspace.hover(params);
    REQUIRE(result);
    CHECK_EQ(result->contents.value,
        codeBlock("luau", "function DocumentedClass:function1(): number") + kDocumentationBreaker + "This is a documented function1 of the class\n");
}

// TEST_CASE_FIXTURE(Fixture, "includes_documentation_when_hovering_over_global_variable_from_definitions_file")
//{
//     auto source = R"(
//         print(DocumentedGlobalVariable)
//     )";
//
//     auto uri = newDocument("foo.luau", source);
//
//     lsp::HoverParams params;
//     params.textDocument = lsp::TextDocumentIdentifier{uri};
//     params.position = lsp::Position{1, 23};
//
//     auto result = workspace.hover(params);
//     REQUIRE(result);
//     CHECK_EQ(result->contents.value,
//         codeBlock("luau", "type DocumentedGlobalVariable = number") + kDocumentationBreaker + "This is a documented global variable\n");
// }


TEST_SUITE_END();
