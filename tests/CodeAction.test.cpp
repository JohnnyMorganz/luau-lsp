#include "doctest.h"
#include "Fixture.h"
#include "Platform/RobloxPlatform.hpp"

static std::optional<lsp::CodeAction> findAction(const lsp::CodeActionResult& result, const std::string& title)
{
    if (!result)
        return std::nullopt;

    for (const auto& action : *result)
    {
        if (action.title == title)
            return action;
    }
    return std::nullopt;
}

TEST_SUITE_BEGIN("CodeAction");

TEST_CASE_FIXTURE(Fixture, "organise_imports_action_is_returned")
{
    auto uri = newDocument("test.luau", R"(
        local b = require("./b.luau")
        local a = require("./a.luau")
    )");

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{0, 0}, {3, 0}};
    params.context.only = {lsp::CodeActionKind::SourceOrganizeImports};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findAction(result, "Sort requires");
    REQUIRE(action.has_value());
    CHECK(action->kind == lsp::CodeActionKind::SourceOrganizeImports);
}

TEST_CASE_FIXTURE(Fixture, "global_used_as_local_quick_fix")
{
    // GlobalUsedAsLocal lint triggers when a global is assigned but never read before being written
    // and is not at module scope (i.e., inside a function)
    auto uri = newDocument("test.luau", R"(
local function foo()
    x = 1
    print(x)
end
)");

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{2, 0}, {2, 10}};
    params.context.only = {lsp::CodeActionKind::QuickFix};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findAction(result, "Add 'local' to global variable");
    REQUIRE(action.has_value());
    CHECK(action->kind == lsp::CodeActionKind::QuickFix);
    CHECK(action->isPreferred == true);
    REQUIRE(action->edit.has_value());
    auto& changes = action->edit->changes.at(uri);
    REQUIRE_EQ(changes.size(), 1);
    CHECK_EQ(changes[0].newText, "local ");
}

TEST_CASE_FIXTURE(Fixture, "quick_fix_only_returns_for_matching_range")
{
    auto uri = newDocument("test.luau", R"(
local function foo()
    x = 1
    print(x)
end
)");

    // Request code actions for a range that doesn't include the lint (line 4 is "end")
    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{4, 0}, {5, 0}};
    params.context.only = {lsp::CodeActionKind::QuickFix};

    auto result = workspace.codeAction(params, nullptr);
    auto action = findAction(result, "Add 'local' to global variable");
    CHECK_FALSE(action.has_value());
}

TEST_CASE_FIXTURE(Fixture, "local_unused_prefix_fix")
{
    auto uri = newDocument("test.luau", R"(
local unused = 1
print("hello")
)");

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{1, 0}, {2, 0}};
    params.context.only = {lsp::CodeActionKind::QuickFix};

    auto result = workspace.codeAction(params, nullptr);

    auto prefixAction = findAction(result, "Prefix 'unused' with '_' to silence");
    REQUIRE(prefixAction.has_value());
    CHECK(prefixAction->kind == lsp::CodeActionKind::QuickFix);
    CHECK(prefixAction->isPreferred == false);
    REQUIRE(prefixAction->edit.has_value());
    auto& prefixChanges = prefixAction->edit->changes.at(uri);
    REQUIRE_EQ(prefixChanges.size(), 1);
    CHECK_EQ(prefixChanges[0].newText, "_");
}

TEST_CASE_FIXTURE(Fixture, "local_unused_delete_fix")
{
    auto uri = newDocument("test.luau", R"(
local unused = 1
print("hello")
)");

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{1, 0}, {2, 0}};
    params.context.only = {lsp::CodeActionKind::QuickFix};

    auto result = workspace.codeAction(params, nullptr);

    auto deleteAction = findAction(result, "Remove unused variable: 'unused'");
    REQUIRE(deleteAction.has_value());
    CHECK(deleteAction->kind == lsp::CodeActionKind::QuickFix);
    CHECK(deleteAction->isPreferred == false);
    REQUIRE(deleteAction->edit.has_value());
    auto& deleteChanges = deleteAction->edit->changes.at(uri);
    REQUIRE_EQ(deleteChanges.size(), 1);
    CHECK_EQ(deleteChanges[0].newText, "");
    CHECK_EQ(deleteChanges[0].range.start.line, 1);
    CHECK_EQ(deleteChanges[0].range.end.line, 2);
}

TEST_CASE_FIXTURE(Fixture, "function_unused_prefix_fix")
{
    auto uri = newDocument("test.luau", R"(
local function unused()
    return 1
end
print("hello")
)");

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{1, 0}, {4, 0}};
    params.context.only = {lsp::CodeActionKind::QuickFix};

    auto result = workspace.codeAction(params, nullptr);

    auto prefixAction = findAction(result, "Prefix 'unused' with '_' to silence");
    REQUIRE(prefixAction.has_value());
    CHECK(prefixAction->kind == lsp::CodeActionKind::QuickFix);
    CHECK(prefixAction->isPreferred == false);
    REQUIRE(prefixAction->edit.has_value());
    auto& prefixChanges = prefixAction->edit->changes.at(uri);
    REQUIRE_EQ(prefixChanges.size(), 1);
    CHECK_EQ(prefixChanges[0].newText, "_");
}

TEST_CASE_FIXTURE(Fixture, "function_unused_delete_fix")
{
    auto uri = newDocument("test.luau", R"(
local function unused()
    return 1
end
print("hello")
)");

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{1, 0}, {4, 0}};
    params.context.only = {lsp::CodeActionKind::QuickFix};

    auto result = workspace.codeAction(params, nullptr);

    auto deleteAction = findAction(result, "Remove unused function: 'unused'");
    REQUIRE(deleteAction.has_value());
    CHECK(deleteAction->kind == lsp::CodeActionKind::QuickFix);
    CHECK(deleteAction->isPreferred == false);
    REQUIRE(deleteAction->edit.has_value());
    auto& deleteChanges = deleteAction->edit->changes.at(uri);
    REQUIRE_EQ(deleteChanges.size(), 1);
    CHECK_EQ(deleteChanges[0].newText, "");
    CHECK_EQ(deleteChanges[0].range.start.line, 1);
    CHECK_EQ(deleteChanges[0].range.end.line, 4);
}

TEST_CASE_FIXTURE(Fixture, "import_unused_prefix_fix")
{
    auto uri = newDocument("test.luau", R"(
local unused = require("./foo.luau")
print("hello")
)");

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{1, 0}, {2, 0}};
    params.context.only = {lsp::CodeActionKind::QuickFix};

    auto result = workspace.codeAction(params, nullptr);

    auto prefixAction = findAction(result, "Prefix 'unused' with '_' to silence");
    REQUIRE(prefixAction.has_value());
    CHECK(prefixAction->kind == lsp::CodeActionKind::QuickFix);
    CHECK(prefixAction->isPreferred == false);
    REQUIRE(prefixAction->edit.has_value());
    auto& prefixChanges = prefixAction->edit->changes.at(uri);
    REQUIRE_EQ(prefixChanges.size(), 1);
    CHECK_EQ(prefixChanges[0].newText, "_");
}

TEST_CASE_FIXTURE(Fixture, "import_unused_delete_fix")
{
    auto uri = newDocument("test.luau", R"(
local unused = require("./foo.luau")
print("hello")
)");

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{1, 0}, {2, 0}};
    params.context.only = {lsp::CodeActionKind::QuickFix};

    auto result = workspace.codeAction(params, nullptr);

    auto deleteAction = findAction(result, "Remove unused import: 'unused'");
    REQUIRE(deleteAction.has_value());
    CHECK(deleteAction->kind == lsp::CodeActionKind::QuickFix);
    CHECK(deleteAction->isPreferred == false);
    REQUIRE(deleteAction->edit.has_value());
    auto& deleteChanges = deleteAction->edit->changes.at(uri);
    REQUIRE_EQ(deleteChanges.size(), 1);
    CHECK_EQ(deleteChanges[0].newText, "");
    CHECK_EQ(deleteChanges[0].range.start.line, 1);
    CHECK_EQ(deleteChanges[0].range.end.line, 2);
}

TEST_CASE_FIXTURE(Fixture, "unreachable_code_fix")
{
    auto uri = newDocument("test.luau", R"(
local function test()
    error("always errors")
    print("unreachable")
end
)");

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{3, 0}, {4, 0}};
    params.context.only = {lsp::CodeActionKind::QuickFix};

    auto result = workspace.codeAction(params, nullptr);

    auto action = findAction(result, "Remove unreachable code");
    REQUIRE(action.has_value());
    CHECK(action->kind == lsp::CodeActionKind::QuickFix);
    CHECK(action->isPreferred == false);
    REQUIRE(action->edit.has_value());
    auto& changes = action->edit->changes.at(uri);
    REQUIRE_EQ(changes.size(), 1);
    CHECK_EQ(changes[0].newText, "");
    CHECK_EQ(changes[0].range.start.line, 3);
    CHECK_EQ(changes[0].range.end.line, 4);
}

TEST_SUITE_END();
