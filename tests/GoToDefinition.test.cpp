#include "doctest.h"
#include "Fixture.h"

#include "LSP/IostreamHelpers.hpp"

TEST_SUITE_BEGIN("GoToDefinition");

TEST_CASE_FIXTURE(Fixture, "local_variable_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local value = 5

        local y = val|ue
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 14});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 19});
}

TEST_CASE_FIXTURE(Fixture, "local_inlined_primitive_table_property_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local T = {
            value = 5
        }

        local y = T.val|ue
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 17});
}

TEST_CASE_FIXTURE(Fixture, "local_separate_primitive_table_property_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local T = {}
        T.value = 5

        local y = T.val|ue
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 10});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 15});
}

TEST_CASE_FIXTURE(Fixture, "local_inlined_function_table_property_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local T = {
            useFunction = function() end
        }

        local y = T.useFu|nction()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 23});
}

TEST_CASE_FIXTURE(Fixture, "local_separate_anonymous_function_table_property_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local T = {}
        T.useFunction = function()
        end

        local y = T.useFu|nction()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 10});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 21});
}

TEST_CASE_FIXTURE(Fixture, "local_named_function_table_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local T = {}
        function T.useFunction()
        end

        local y = T.useFu|nction()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 19});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 30});
}

TEST_CASE_FIXTURE(Fixture, "type_alias_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        type Foo = string

        local y: Fo|o = ""
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 8});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 25});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_inlined_function_table_property_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        return {
            useFunction = function(x: string)
            end,
        }
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 23});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_indirect_variable_inlined_function_table_property_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        local T = {
            useFunction = function(x: string)
            end,
        }
        return T
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 23});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_table_property_referencing_local_variable_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        local function useFunction(x: string)
        end

        return {
            useFunction = useFunction
        }
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{6, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{6, 23});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_separate_function_table_property_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        local T = {}
        T.useFunction = function(x: string)
        end

        return T
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 10});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 21});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_named_function_table_property_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        local T = {}
        function T.useFunction(x: string)
        end

        return T
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 19});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 30});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_imported_type_alias_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        export type Foo = string

        return {}
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y: utilities.Fo|o = ""
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 8});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 32});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_of_a_named_function_returns_the_underlying_definition_and_not_the_require_stmt")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        local function useFunction(x: string)
        end

        return useFunction
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local useFunction = require(game.Testing.useFunction)

        local y = useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 23});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 34});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_of_an_anonymous_function_returns_the_underlying_definition_and_not_the_require_stmt")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        return function(x: string)
        end
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local useFunction = require(game.Testing.useFunction)

        local y = useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 15});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 15});
}

TEST_SUITE_END();
