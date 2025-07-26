#include "doctest.h"
#include "Fixture.h"

static std::string applyEdit(const std::string& source, const std::vector<lsp::TextEdit>& edits)
{
    std::string newSource;

    lsp::Position currentPos{0, 0};
    std::optional<lsp::Position> editEndPos = std::nullopt;
    for (const auto& c : source)
    {
        if (c == '\n')
        {
            currentPos.line += 1;
            currentPos.character = 0;
        }
        else
        {
            currentPos.character += 1;
        }

        if (editEndPos)
        {
            if (currentPos == *editEndPos)
                editEndPos = std::nullopt;
        }
        else
            newSource += c;

        for (const auto& edit : edits)
        {
            if (currentPos == edit.range.start)
            {
                newSource += edit.newText;
                editEndPos = edit.range.end;
            }
        }
    }

    return newSource;
}

TEST_SUITE_BEGIN("Rename");

TEST_CASE_FIXTURE(Fixture, "fail_if_new_name_is_empty")
{
    auto uri = newDocument("foo.luau", "");

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{0, 0};
    params.newName = "";

    REQUIRE_THROWS_WITH_AS(workspace.rename(params, nullptr), "The new name must be a valid identifier", JsonRpcException);
}

TEST_CASE_FIXTURE(Fixture, "fail_if_new_name_does_not_start_as_valid_identifier")
{
    auto uri = newDocument("foo.luau", "");

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{0, 0};
    params.newName = "1234";

    REQUIRE_THROWS_WITH_AS(
        workspace.rename(params, nullptr), "The new name must be a valid identifier starting with a character or underscore", JsonRpcException);
}

TEST_CASE_FIXTURE(Fixture, "fail_if_new_name_is_not_a_valid_identifier")
{
    auto uri = newDocument("foo.luau", "");

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{0, 0};
    params.newName = "testing123!";

    REQUIRE_THROWS_WITH_AS(workspace.rename(params, nullptr),
        "The new name must be a valid identifier composed of characters, digits, and underscores only", JsonRpcException);
}

TEST_CASE_FIXTURE(Fixture, "rename_generic_type_parameter")
{
    // https://github.com/JohnnyMorganz/luau-lsp/issues/488
    // Renaming: "S" inside of <S>

    auto source = R"(
        type LayerBuilder<S> = S & { ... }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 26};
    params.newName = "State";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK(applyEdit(source, documentEdits) == R"(
        type LayerBuilder<State> = State & { ... }
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_generic_type_parameter_used_inside_of_type_alias")
{
    // https://github.com/JohnnyMorganz/luau-lsp/issues/488
    // Renaming: "S" inside of assignment `S & { ... }`

    auto source = R"(
        type LayerBuilder<S> = S & { ... }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 31};
    params.newName = "State";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        type LayerBuilder<State> = State & { ... }
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_generic_type_parameter_used_as_another_generic")
{
    auto source = R"(
        type Baz<S...> = Bar<S...>
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 17};
    params.newName = "State";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        type Baz<State...> = Bar<State...>
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_generic_type_parameter_in_function_definition")
{
    auto source = R"(
        function foo<T>(x: T, y: string)
        end
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 21};
    params.newName = "Value";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        function foo<Value>(x: Value, y: string)
        end
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_generic_type_correctly_handle_shadowing_1")
{
    // Renaming "T" in Foo<T>
    auto source = R"(
        type Foo<T> = {
            x: T,
            fn: <T>(T, T) -> T
        }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{2, 15};
    params.newName = "Value";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        type Foo<Value> = {
            x: Value,
            fn: <T>(T, T) -> T
        }
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_generic_type_correctly_handle_shadowing_2")
{
    // Renaming first "T" in <T>(T, T) -> T
    auto source = R"(
        type Foo<T> = {
            x: T,
            fn: <T>(T, T) -> T
        }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{3, 17};
    params.newName = "Value";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        type Foo<T> = {
            x: T,
            fn: <Value>(Value, Value) -> Value
        }
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_generic_type_correctly_handle_shadowing_3")
{
    // Renaming second "T" in <T>(T, T) -> T
    auto source = R"(
        type Foo<T> = {
            x: T,
            fn: <T>(T, T) -> T
        }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{3, 20};
    params.newName = "Value";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        type Foo<T> = {
            x: T,
            fn: <Value>(Value, Value) -> Value
        }
    )");
}

TEST_CASE_FIXTURE(Fixture, "renaming_required_variable_should_also_rename_imported_type_prefixes")
{
    auto source = R"(
        local Types = require("path/to/types")

        type Foo = Types.Foo
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 14};
    params.newName = "ActualTypes";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        local ActualTypes = require("path/to/types")

        type Foo = ActualTypes.Foo
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_global_function_name")
{
    auto source = R"(
        function Main()
        end

        Main()
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{4, 11};
    params.newName = "Test";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        function Test()
        end

        Test()
    )");
}

TEST_CASE_FIXTURE(Fixture, "disallow_renaming_of_global_from_type_definition")
{
    auto source = R"(
        local x = game:GetService("Foo")
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 19}; // 'game'
    params.newName = "game2";

    REQUIRE_THROWS_WITH_AS(workspace.rename(params, nullptr), "Cannot rename a global variable", JsonRpcException);
}

TEST_CASE_FIXTURE(Fixture, "dont_rename_cross_module_usages_of_a_returned_local_function")
{
    auto uri = newDocument("useFunction.luau", R"(
        local function useFunction()
        end

        return useFunction
    )");

    auto user = newDocument("user.luau", R"(
        local useFunction = require("useFunction.luau")

        local value = useFunction()
    )");

    workspace.frontend.parse(workspace.fileResolver.getModuleName(user));

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 27}; // 'useFunction' definition
    params.newName = "useFunction2";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->changes.size(), 1);
    CHECK_EQ(result->changes.begin()->first, uri);
    CHECK_EQ(result->changes.begin()->second.size(), 2);
}

TEST_CASE_FIXTURE(Fixture, "dont_rename_cross_module_usages_of_a_returned_global_function")
{
    auto uri = newDocument("useFunction.luau", R"(
        function useFunction()
        end

        return useFunction
    )");

    auto user = newDocument("user.luau", R"(
        local useFunction = require("useFunction.luau")

        local value = useFunction()
    )");

    workspace.frontend.parse(workspace.fileResolver.getModuleName(user));

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 20}; // 'useFunction' definition
    params.newName = "useFunction2";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->changes.size(), 1);
    CHECK_EQ(result->changes.begin()->first, uri);
    CHECK_EQ(result->changes.begin()->second.size(), 2);
}

TEST_CASE_FIXTURE(Fixture, "dont_rename_cross_module_usages_of_a_returned_table")
{
    auto uri = newDocument("tbl.luau", R"(
        local tbl = {}

        return tbl
    )");

    auto user = newDocument("user.luau", R"(
        local tbl = require("tbl.luau")

        local value = tbl
    )");

    workspace.frontend.parse(workspace.fileResolver.getModuleName(user));

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 15}; // 'tbl' definition
    params.newName = "tbl2";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->changes.size(), 1);
    CHECK_EQ(result->changes.begin()->first, uri);
    CHECK_EQ(result->changes.begin()->second.size(), 2);
}

TEST_CASE_FIXTURE(Fixture, "response_json_is_valid_structure")
{
    auto uri = newDocument("tbl.luau", R"(
        local value = 1
    )");

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 15}; // 'value' definition
    params.newName = "value2";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->changes.size(), 1);

    json response = result;
    CHECK_EQ(response.dump(), R"({"changes":{")" + uri.toString() +
                                  R"(":[{"newText":"value2","range":{"end":{"character":19,"line":1},"start":{"character":14,"line":1}}}]}})");
}

TEST_SUITE_END();
