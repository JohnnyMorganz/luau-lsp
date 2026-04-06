#include "doctest.h"
#include "Fixture.h"

TEST_SUITE_BEGIN("Refactoring");

// Extract Variable

TEST_CASE_FIXTURE(Fixture, "extract_variable_simple_expression")
{
    auto source = R"(
local a = 1
local b = 2
local x = a + b
)";
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    // Select "a + b" on line 3 (0-indexed), columns 10-15
    params.range = {{3, 10}, {3, 15}};
    params.context.only = {lsp::CodeActionKind::RefactorExtract};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findCodeAction(result, "Extract to local variable");
    REQUIRE(action);
    CHECK(action->kind == lsp::CodeActionKind::RefactorExtract);
    REQUIRE(action->data);

    // Resolve the action to get the edit
    auto resolved = workspace.codeActionResolve(*action, nullptr);
    REQUIRE(resolved.edit);

    auto& changes = resolved.edit->changes.at(uri);
    auto newSource = applyEdit(source, changes);
    CHECK_EQ(newSource, R"(
local a = 1
local b = 2
local extracted = a + b
local x = extracted
)");

    // Should trigger rename at the new variable name
    REQUIRE(resolved.command);
    CHECK_EQ(resolved.command->command, "editor.action.rename");
}

TEST_CASE_FIXTURE(Fixture, "extract_variable_function_call")
{
    auto source = R"(
local x = tostring(42)
)";
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    // Select "tostring(42)" on line 1, columns 10-22
    params.range = {{1, 10}, {1, 22}};
    params.context.only = {lsp::CodeActionKind::RefactorExtract};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findCodeAction(result, "Extract to local variable");
    REQUIRE(action);

    auto resolved = workspace.codeActionResolve(*action, nullptr);
    REQUIRE(resolved.edit);

    auto& changes = resolved.edit->changes.at(uri);
    auto newSource = applyEdit(source, changes);
    CHECK_EQ(newSource, R"(
local extracted = tostring(42)
local x = extracted
)");
}

TEST_CASE_FIXTURE(Fixture, "extract_variable_not_offered_for_local_ref")
{
    auto source = R"(
local x = 1
print(x)
)";
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    // Select just "x" on line 2
    params.range = {{2, 6}, {2, 7}};
    params.context.only = {lsp::CodeActionKind::RefactorExtract};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findCodeAction(result, "Extract to local variable");
    CHECK_FALSE(action);
}

// Extract Function

TEST_CASE_FIXTURE(Fixture, "extract_function_single_statement")
{
    auto source = R"(
local x = 1
print(x)
local y = 2
)";
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    // Select "print(x)" on line 2
    params.range = {{2, 0}, {2, 8}};
    params.context.only = {lsp::CodeActionKind::RefactorExtract};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findCodeAction(result, "Extract to function");
    REQUIRE(action);
    CHECK(action->kind == lsp::CodeActionKind::RefactorExtract);

    auto resolved = workspace.codeActionResolve(*action, nullptr);
    REQUIRE(resolved.edit);

    auto& changes = resolved.edit->changes.at(uri);
    auto newSource = applyEdit(source, changes);
    CHECK_EQ(newSource, R"(
local x = 1
local function extracted(x)
    print(x)
end
extracted(x)
local y = 2
)");
}

TEST_CASE_FIXTURE(Fixture, "extract_function_with_return_values")
{
    auto source = R"(
local a = 1
local b = a + 1
print(b)
)";
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    // Select "local b = a + 1" on line 2
    params.range = {{2, 0}, {2, 15}};
    params.context.only = {lsp::CodeActionKind::RefactorExtract};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findCodeAction(result, "Extract to function");
    REQUIRE(action);

    auto resolved = workspace.codeActionResolve(*action, nullptr);
    REQUIRE(resolved.edit);

    auto& changes = resolved.edit->changes.at(uri);
    auto newSource = applyEdit(source, changes);
    CHECK_EQ(newSource, R"(
local a = 1
local function extracted(a)
    local b = a + 1
    return b
end
local b = extracted(a)
print(b)
)");
}

TEST_CASE_FIXTURE(Fixture, "extract_function_rejects_return_statement")
{
    auto source = R"(
local function foo()
    local x = 1
    return x
end
)";
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    // Select "return x" on line 3
    params.range = {{3, 0}, {3, 12}};
    params.context.only = {lsp::CodeActionKind::RefactorExtract};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findCodeAction(result, "Extract to function");
    CHECK_FALSE(action);
}

TEST_CASE_FIXTURE(Fixture, "extract_function_allows_loop_with_break")
{
    auto source = R"(
for i = 1, 10 do
    if i == 5 then break end
    print(i)
end
)";
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    // Select the entire for loop (lines 1-4)
    params.range = {{1, 0}, {4, 3}};
    params.context.only = {lsp::CodeActionKind::RefactorExtract};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findCodeAction(result, "Extract to function");
    // break inside the loop should be allowed since it doesn't escape the selection
    REQUIRE(action);
}

TEST_CASE_FIXTURE(Fixture, "extract_function_allows_callback_with_return")
{
    auto source = R"(
local t = {3, 1, 2}
table.sort(t, function(a, b) return a < b end)
print(t)
)";
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    // Select the table.sort call on line 2
    params.range = {{2, 0}, {2, 47}};
    params.context.only = {lsp::CodeActionKind::RefactorExtract};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findCodeAction(result, "Extract to function");
    // return inside the callback should not be flagged as a control flow escape
    REQUIRE(action);
}

// Inline Variable

TEST_CASE_FIXTURE(Fixture, "inline_variable_simple")
{
    auto source = R"(
local x = 1
print(x)
)";
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    // Cursor on "x" in the declaration (line 1, col 6)
    params.range = {{1, 6}, {1, 7}};
    params.context.only = {lsp::CodeActionKind::RefactorInline};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findCodeAction(result, "Inline variable 'x'");
    REQUIRE(action);
    CHECK(action->kind == lsp::CodeActionKind::RefactorInline);

    auto resolved = workspace.codeActionResolve(*action, nullptr);
    REQUIRE(resolved.edit);

    auto& changes = resolved.edit->changes.at(uri);
    auto newSource = applyEdit(source, changes);
    CHECK_EQ(newSource, R"(
print(1)
)");
}

TEST_CASE_FIXTURE(Fixture, "inline_variable_from_usage_site")
{
    auto source = R"(
local x = 1
print(x)
)";
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    // Cursor on "x" at the usage site (line 2, col 6)
    params.range = {{2, 6}, {2, 7}};
    params.context.only = {lsp::CodeActionKind::RefactorInline};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findCodeAction(result, "Inline variable 'x'");
    REQUIRE(action);

    auto resolved = workspace.codeActionResolve(*action, nullptr);
    REQUIRE(resolved.edit);

    auto& changes = resolved.edit->changes.at(uri);
    auto newSource = applyEdit(source, changes);
    CHECK_EQ(newSource, R"(
print(1)
)");
}

TEST_CASE_FIXTURE(Fixture, "inline_variable_adds_parens")
{
    auto source = R"(
local a = 1
local b = 2
local x = a + b
local y = x * 3
)";
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    // Cursor on "x" in "local x = a + b"
    params.range = {{3, 6}, {3, 7}};
    params.context.only = {lsp::CodeActionKind::RefactorInline};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findCodeAction(result, "Inline variable 'x'");
    REQUIRE(action);

    auto resolved = workspace.codeActionResolve(*action, nullptr);
    REQUIRE(resolved.edit);

    auto& changes = resolved.edit->changes.at(uri);
    auto newSource = applyEdit(source, changes);
    CHECK_EQ(newSource, R"(
local a = 1
local b = 2
local y = (a + b) * 3
)");
}

TEST_CASE_FIXTURE(Fixture, "inline_variable_multiple_references")
{
    auto source = R"(
local x = 42
print(x)
print(x)
)";
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{1, 6}, {1, 7}};
    params.context.only = {lsp::CodeActionKind::RefactorInline};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findCodeAction(result, "Inline variable 'x'");
    REQUIRE(action);

    auto resolved = workspace.codeActionResolve(*action, nullptr);
    REQUIRE(resolved.edit);

    auto& changes = resolved.edit->changes.at(uri);
    auto newSource = applyEdit(source, changes);
    CHECK_EQ(newSource, R"(
print(42)
print(42)
)");
}

TEST_CASE_FIXTURE(Fixture, "inline_variable_resolve_rejects_multi_var")
{
    auto source = R"(
local a, b = 1, 2
print(a)
)";
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{1, 6}, {1, 7}};
    params.context.only = {lsp::CodeActionKind::RefactorInline};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findCodeAction(result, "Inline variable 'a'");
    REQUIRE(action);

    // Resolve should produce no edit for multi-var declarations
    auto resolved = workspace.codeActionResolve(*action, nullptr);
    CHECK_FALSE(resolved.edit);
}

TEST_CASE_FIXTURE(Fixture, "inline_variable_resolve_rejects_no_init")
{
    auto source = R"(
local x
x = 1
print(x)
)";
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{1, 6}, {1, 7}};
    params.context.only = {lsp::CodeActionKind::RefactorInline};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findCodeAction(result, "Inline variable 'x'");
    REQUIRE(action);

    // Resolve should produce no edit when there is no initializer
    auto resolved = workspace.codeActionResolve(*action, nullptr);
    CHECK_FALSE(resolved.edit);
}

TEST_CASE_FIXTURE(Fixture, "inline_variable_resolve_rejects_reassigned")
{
    auto source = R"(
local x = 1
x = 2
print(x)
)";
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{1, 6}, {1, 7}};
    params.context.only = {lsp::CodeActionKind::RefactorInline};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findCodeAction(result, "Inline variable 'x'");
    REQUIRE(action);

    // Resolve should produce no edit when variable is reassigned
    auto resolved = workspace.codeActionResolve(*action, nullptr);
    CHECK_FALSE(resolved.edit);
}

TEST_SUITE_END();
