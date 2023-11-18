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

    auto result = workspace.rename(params);
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

    auto result = workspace.rename(params);
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

    auto result = workspace.rename(params);
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

    auto result = workspace.rename(params);
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

    auto result = workspace.rename(params);
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

    auto result = workspace.rename(params);
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

    auto result = workspace.rename(params);
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

TEST_SUITE_END();
