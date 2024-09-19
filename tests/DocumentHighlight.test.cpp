#include "doctest.h"
#include "Fixture.h"

#include "LSP/IostreamHelpers.hpp"

TEST_SUITE_BEGIN("DocumentHighlight");

TEST_CASE_FIXTURE(Fixture, "return_nullopt_on_no_highlights")
{
    auto uri = newDocument("foo.luau", "");

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{0, 0}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result == std::nullopt);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_generic_type_parameter")
{
    auto source = R"(
        type LayerBuilder<S> = S & { ... }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 26}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 2);
    REQUIRE(highlights[0].range == lsp::Range{{1, 26}, {1, 27}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{1, 31}, {1, 32}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_generic_type_parameter_used_inside_of_type_alias")
{
    auto source = R"(
        type LayerBuilder<S> = S & { ... }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 31}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 2);
    REQUIRE(highlights[0].range == lsp::Range{{1, 26}, {1, 27}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{1, 31}, {1, 32}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_generic_type_parameter_used_as_another_generic")
{
    // Finding the highlights of `S` in `Baz<S...>`, given the position of said `S`
    auto source = R"(
        type Baz<S...> = Bar<S...>
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 17}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 2);
    REQUIRE(highlights[0].range == lsp::Range{{1, 17}, {1, 18}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{1, 29}, {1, 30}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_generic_type_parameter_in_function_definition")
{
    // Finding the highlights of `T` in foo<T>, given the position of said `T`
    auto source = R"(
        function foo<T>(x: T, y: string)
        end
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 21}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 2);
    REQUIRE(highlights[0].range == lsp::Range{{1, 21}, {1, 22}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{1, 27}, {1, 28}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_generic_type_correctly_handling_shadowing_1")
{
    // Finding highlights of T in Foo<T>, given the position of T in `x: T`
    auto source = R"(
        type Foo<T> = {
            x: T,
            fn: <T>(T, T) -> T
        }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{2, 15}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 2);
    REQUIRE(highlights[0].range == lsp::Range{{1, 17}, {1, 18}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{2, 15}, {2, 16}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_generic_type_correctly_handling_shadowing_2")
{
    // Finding highlights of the first `T` in `<T>(T, T) -> T`
    auto source = R"(
        type Foo<T> = {
            x: T,
            fn: <T>(T, T) -> T
        }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{3, 17}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 4);
    REQUIRE(highlights[0].range == lsp::Range{{3, 17}, {3, 18}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{3, 20}, {3, 21}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
    REQUIRE(highlights[2].range == lsp::Range{{3, 23}, {3, 24}});
    REQUIRE(highlights[2].kind == lsp::DocumentHighlightKind::Read);
    REQUIRE(highlights[3].range == lsp::Range{{3, 29}, {3, 30}});
    REQUIRE(highlights[3].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_generic_type_correctly_handling_shadowing_3")
{
    // Finding highlights of the first `T` in `<T>(T, T) -> T`
    auto source = R"(
        type Foo<T> = {
            x: T,
            fn: <T>(T, T) -> T
        }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{3, 20}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 4);
    REQUIRE(highlights[0].range == lsp::Range{{3, 17}, {3, 18}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{3, 20}, {3, 21}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
    REQUIRE(highlights[2].range == lsp::Range{{3, 23}, {3, 24}});
    REQUIRE(highlights[2].kind == lsp::DocumentHighlightKind::Read);
    REQUIRE(highlights[3].range == lsp::Range{{3, 29}, {3, 30}});
    REQUIRE(highlights[3].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "finding_highlights_of_required_variable_should_also_find_imported_type_prefixes")
{
    auto source = R"(
        local Types = require("path/to/types")

        type Foo = Types.Foo
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 14}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 2);
    REQUIRE(highlights[0].range == lsp::Range{{1, 14}, {1, 19}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{3, 19}, {3, 24}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "finding_highlights_of_required_variable_should_also_find_imported_type_prefixes_2")
{
    auto source = R"(
        local Types = require("path/to/types")
        type Foo = Types.Foo
        do
            local Types = "shadow"
            type Bar = Types.Bar
            local Types = require("path/to/types")
            local Types = "shadow"
            type Foo = Types.Foo
        end
        type Bar = Types.Bar
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 14}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 4);
    REQUIRE(highlights[0].range == lsp::Range{{1, 14}, {1, 19}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{2, 19}, {2, 24}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
    REQUIRE(highlights[2].range == lsp::Range{{5, 23}, {5, 28}});
    REQUIRE(highlights[2].kind == lsp::DocumentHighlightKind::Read);
    REQUIRE(highlights[3].range == lsp::Range{{10, 19}, {10, 24}});
    REQUIRE(highlights[3].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "finding_highlights_of_required_variable_should_also_find_imported_type_prefixes_3")
{
    auto source = R"(
        local Types = require("path/to/types")
        type Foo = Types.Foo
        do
            local Types = "shadow"
            type Bar = Types.Bar
            local Types = require("path/to/types")
            local Types = "shadow"
            type Foo = Types.Foo
        end
        type Bar = Types.Bar
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{8, 23}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 2);
    REQUIRE(highlights[0].range == lsp::Range{{6, 18}, {6, 23}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{8, 23}, {8, 28}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_global_function")
{
    auto source = R"(
        function Main()
        end

        Main()
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{4, 11}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 2);
    REQUIRE(highlights[0].range == lsp::Range{{1, 17}, {1, 21}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{4, 8}, {4, 12}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_global_function_2")
{
    auto source = R"(
        local module = {}
        function module.Test() end
        module.Foo = {}
        function module.Foo.Test() end
        module.Test()
        module.Foo.Test()
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{5, 15}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 2);
    REQUIRE(highlights[0].range == lsp::Range{{2, 24}, {2, 28}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{5, 15}, {5, 19}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_global_function_3")
{
    auto source = R"(
        local module = {}
        function module.Test() end
        module.Foo = {}
        function module.Foo.Test() end
        module.Test()
        module.Foo.Test()
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{4, 28}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 2);
    REQUIRE(highlights[0].range == lsp::Range{{4, 28}, {4, 32}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{6, 19}, {6, 23}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_global_function_4")
{
    auto source = R"(
        local module = {}
        module.Foo = {}
        function module.Foo:Test() end
        module.Foo:Test()
        module.Foo.Test(module.Foo)
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{4, 20}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 3);
    REQUIRE(highlights[0].range == lsp::Range{{3, 28}, {3, 32}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{4, 19}, {4, 23}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
    REQUIRE(highlights[2].range == lsp::Range{{5, 19}, {5, 23}});
    REQUIRE(highlights[2].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_for_in_variables")
{
    auto source = R"(
        for i, v in {"foo", "bar"} do
            print(i)
            i += 4
            print(v)
            v = i
        end
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 12}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 4);
    REQUIRE(highlights[0].range == lsp::Range{{1, 12}, {1, 13}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{2, 18}, {2, 19}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
    REQUIRE(highlights[2].range == lsp::Range{{3, 12}, {3, 13}});
    REQUIRE(highlights[2].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[3].range == lsp::Range{{5, 16}, {5, 17}});
    REQUIRE(highlights[3].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_for_in_variables_2")
{
    auto source = R"(
        for i, v in {"foo", "bar"} do
            print(i)
            i += 4
            print(v)
            v = i
        end
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 15}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 3);
    REQUIRE(highlights[0].range == lsp::Range{{1, 15}, {1, 16}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{4, 18}, {4, 19}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
    REQUIRE(highlights[2].range == lsp::Range{{5, 12}, {5, 13}});
    REQUIRE(highlights[2].kind == lsp::DocumentHighlightKind::Write);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_symbol_with_assignment")
{
    auto source = R"(
        local s = 12
        s += 4
        s = s + 4
        print(s)
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{3, 8}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 5);
    REQUIRE(highlights[0].range == lsp::Range{{1, 14}, {1, 15}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{2, 8}, {2, 9}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[2].range == lsp::Range{{3, 8}, {3, 9}});
    REQUIRE(highlights[2].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[3].range == lsp::Range{{3, 12}, {3, 13}});
    REQUIRE(highlights[3].kind == lsp::DocumentHighlightKind::Read);
    REQUIRE(highlights[4].range == lsp::Range{{4, 14}, {4, 15}});
    REQUIRE(highlights[4].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_uninitialized_local")
{
    auto source = R"(
        local s
        s = 4
        print(s)
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{3, 14}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 3);
    REQUIRE(highlights[0].range == lsp::Range{{1, 14}, {1, 15}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{2, 8}, {2, 9}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[2].range == lsp::Range{{3, 14}, {3, 15}});
    REQUIRE(highlights[2].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_table_with_assigment")
{
    auto source = R"(
        local tbl = {}
        tbl.foo = "bar"
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 14}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 2);
    REQUIRE(highlights[0].range == lsp::Range{{1, 14}, {1, 17}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{2, 8}, {2, 11}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_assigned_property")
{
    auto source = R"(
        local tbl = {
            foo = "bar"
        }
        tbl.foo = "baz"
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{2, 12}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 2);
    REQUIRE(highlights[0].range == lsp::Range{{2, 12}, {2, 15}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{4, 12}, {4, 15}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Write);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_recursive_property")
{
    auto source = R"(
        local tbl = {}
        tbl.foo = tbl
        print(tbl.foo.foo)
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{2, 12}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 3);
    REQUIRE(highlights[0].range == lsp::Range{{2, 12}, {2, 15}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{3, 22}, {3, 25}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
    REQUIRE(highlights[2].range == lsp::Range{{3, 18}, {3, 21}});
    REQUIRE(highlights[2].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_recursive_property_with_assignment")
{
    auto source = R"(
        local tbl = {}
        tbl.foo = tbl
        tbl.foo.foo = tbl.foo
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{2, 12}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 4);
    REQUIRE(highlights[0].range == lsp::Range{{2, 12}, {2, 15}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{3, 16}, {3, 19}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[2].range == lsp::Range{{3, 12}, {3, 15}});
    REQUIRE(highlights[2].kind == lsp::DocumentHighlightKind::Read);
    REQUIRE(highlights[3].range == lsp::Range{{3, 26}, {3, 29}});
    REQUIRE(highlights[3].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_global")
{
    auto source = R"(
        g = 4
        print(g)
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 8}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 2);
    REQUIRE(highlights[0].range == lsp::Range{{1, 8}, {1, 9}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{2, 14}, {2, 15}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_global_2")
{
    auto source = R"(
        print(g)
        do
            g = 12
        end
        g += 4
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 14}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 3);
    REQUIRE(highlights[0].range == lsp::Range{{1, 14}, {1, 15}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Read);
    REQUIRE(highlights[1].range == lsp::Range{{3, 12}, {3, 13}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[2].range == lsp::Range{{5, 8}, {5, 9}});
    REQUIRE(highlights[2].kind == lsp::DocumentHighlightKind::Write);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_arg_with_shadowing")
{
    auto source = R"(
        local foo = {}
        local function foo(foo)
            print(foo)
        end
        print(foo)
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{2, 27}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 2);
    REQUIRE(highlights[0].range == lsp::Range{{2, 27}, {2, 30}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{3, 18}, {3, 21}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
}

TEST_CASE_FIXTURE(Fixture, "find_highlights_of_local_func_with_shadowing")
{
    auto source = R"(
        local foo = {}
        local function foo(foo)
            print(foo)
        end
        print(foo)
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::DocumentHighlightParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{2, 26}};
    auto result = workspace.documentHighlight(params);
    REQUIRE(result);
    auto highlights = result.value();

    REQUIRE(highlights.size() == 2);
    REQUIRE(highlights[0].range == lsp::Range{{2, 23}, {2, 26}});
    REQUIRE(highlights[0].kind == lsp::DocumentHighlightKind::Write);
    REQUIRE(highlights[1].range == lsp::Range{{5, 14}, {5, 17}});
    REQUIRE(highlights[1].kind == lsp::DocumentHighlightKind::Read);
}

TEST_SUITE_END();