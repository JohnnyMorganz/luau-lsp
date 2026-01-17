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

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 14});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 19});
}

TEST_CASE_FIXTURE(Fixture, "local_variable_definition_pointing_to_a_table")
{
    auto [source, position] = sourceWithMarker(R"(
        local process = { call = function() end }
        local value = pro|cess.call()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{1, 14});
    CHECK_EQ(result[0].range.end, lsp::Position{1, 21});
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

    auto result = workspace.gotoDefinition(params, nullptr);
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

    auto result = workspace.gotoDefinition(params, nullptr);
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

    auto result = workspace.gotoDefinition(params, nullptr);
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

    auto result = workspace.gotoDefinition(params, nullptr);
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

    auto result = workspace.gotoDefinition(params, nullptr);
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

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 8});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 25});
}

TEST_CASE_FIXTURE(Fixture, "methods_on_explicitly_defined_self")
{
    auto [source, position] = sourceWithMarker(R"(
        local Test = {}
        Test.__index = Test

        function Test.new()
            local self = setmetatable({}, Test)
            return self
        end

        function Test.someFunc(self: Test)
        end

        function Test:anotherFunc()
        end

        function Test.anotherCallingFunc(self: Test)
            self:some|Func()
            self:anotherFunc()
        end

        export type Test = typeof(Test.new())

        return Test
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{9, 22});
    CHECK_EQ(result[0].range.end, lsp::Position{9, 30});

    params.position.line += 1;
    result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{12, 22});
    CHECK_EQ(result[0].range.end, lsp::Position{12, 33});
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

    auto result = workspace.gotoDefinition(params, nullptr);
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

    auto result = workspace.gotoDefinition(params, nullptr);
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

    auto result = workspace.gotoDefinition(params, nullptr);
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

    auto result = workspace.gotoDefinition(params, nullptr);
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

    auto result = workspace.gotoDefinition(params, nullptr);
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

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 8});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 32});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_imported_type_alias_definition_after_check_without_retaining_type_graphs")
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

    // This test explicitly expects type graphs to not be retained (i.e., the required module scope was cleared)
    // We should still be able to find the type references.
    workspace.checkSimple(workspace.fileResolver.getModuleName(document), /* cancellationToken= */ nullptr);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 8});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 32});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_imported_type_alias_definition_after_check_without_retaining_type_graphs_goto_type_definition")
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

    // This test explicitly expects type graphs to not be retained (i.e., the required module scope was cleared)
    // We should still be able to find the type references.
    workspace.checkSimple(workspace.fileResolver.getModuleName(document), /* cancellationToken= */ nullptr);

    auto params = lsp::TypeDefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoTypeDefinition(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->uri, required);
    CHECK_EQ(result->range.start, lsp::Position{2, 8});
    CHECK_EQ(result->range.end, lsp::Position{2, 32});
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

    auto result = workspace.gotoDefinition(params, nullptr);
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

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 15});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 15});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_works_for_a_string_require_path")
{
    auto [source, position] = sourceWithMarker(R"(
        local X = require("./te|st")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, workspace.rootUri.resolvePath("test.lua"));
    CHECK_EQ(result[0].range.start, lsp::Position{0, 0});
    CHECK_EQ(result[0].range.end, lsp::Position{0, 0});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_works_for_a_roblox_require_path")
{
    loadSourcemap(R"({
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "children": [{ "name": "Test", "className": "ModuleScript", "filePaths": ["source.luau"] }]
            }
        ]
    })");

    auto [source, position] = sourceWithMarker(R"(
        local X = require(game.ReplicatedStorage.Te|st)
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, workspace.rootUri.resolvePath("source.luau"));
    CHECK_EQ(result[0].range.start, lsp::Position{0, 0});
    CHECK_EQ(result[0].range.end, lsp::Position{0, 0});
}

TEST_CASE_FIXTURE(Fixture, "property_on_table_type_without_actual_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        type Process = {
            spawn: (string) -> string
        }

        local process = {} :: Process
        local value = process.sp|awn()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 17});
}

TEST_CASE_FIXTURE(Fixture, "property_on_the_return_of_a_function_call")
{
    auto [source, position] = sourceWithMarker(R"(
        type Result = {
            unwrap: (Result) -> boolean
        }

        type Process = {
            spawn: (string) -> Result
        }

        local process = {} :: Process
        local value = process.spawn("test"):unwr|ap()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 18});
}

TEST_CASE_FIXTURE(Fixture, "go_to_type_definition_returns_the_table_location")
{
    auto [source, position] = sourceWithMarker(R"(
        type Process = {
            spawn: (string) -> Result
        }

        local process = {} :: Process
        local value = pr|ocess.spawn("test")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::TypeDefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoTypeDefinition(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->uri, document);
    CHECK_EQ(result->range.start, lsp::Position{1, 23});
    CHECK_EQ(result->range.end, lsp::Position{3, 9});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_respects_cancellation")
{
    auto cancellationToken = std::make_shared<Luau::FrontendCancellationToken>();
    cancellationToken->cancel();

    auto document = newDocument("a.luau", "local x = 1");
    CHECK_THROWS_AS(workspace.gotoDefinition(lsp::DefinitionParams{{{document}}}, cancellationToken), RequestCancelledException);
}

TEST_CASE_FIXTURE(Fixture, "go_to_type_definition_respects_cancellation")
{
    auto cancellationToken = std::make_shared<Luau::FrontendCancellationToken>();
    cancellationToken->cancel();

    auto document = newDocument("a.luau", "local x = 1");
    CHECK_THROWS_AS(workspace.gotoTypeDefinition(lsp::TypeDefinitionParams{{{document}}}, cancellationToken), RequestCancelledException);
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_for_property_on_union_type_with_same_definition")
{
    auto document = newDocument("main.luau", R"(
        type BaseNode<HOS> = {
            has_one_supporter: HOS,
        }

        type Node = BaseNode<true> | BaseNode<false>

        local x: Node = {} :: any
        local y = x.has_one_supporter
    )");

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = lsp::Position{8, 20}; // x.has_one_supporter

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range, lsp::Range{{2, 12}, {2, 29}});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_for_property_on_union_type_with_different_definitions")
{
    auto document = newDocument("main.luau", R"(
        type A = {
            prop: number,
        }

        type B = {
            prop: string,
        }

        type Union = A | B

        local x: Union = {} :: any
        local y = x.prop
    )");

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = lsp::Position{12, 20}; // x.prop

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 2);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[1].uri, document);
    CHECK_EQ(result[0].range, lsp::Range{{2, 12}, {2, 16}});
    CHECK_EQ(result[1].range, lsp::Range{{6, 12}, {6, 16}});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_on_original_global_function_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        function global|Function()
        end
    )");

    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range, lsp::Range{{1, 17}, {1, 31}});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_on_original_local_function_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        local function local|Function()
        end
    )");

    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range, lsp::Range{{1, 23}, {1, 36}});
}

TEST_SUITE_END();
