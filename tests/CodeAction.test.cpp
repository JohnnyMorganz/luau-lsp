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
    REQUIRE(action);
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
    REQUIRE(action);
    CHECK(action->kind == lsp::CodeActionKind::QuickFix);
    CHECK(action->isPreferred == true);
    REQUIRE(action->edit);
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
    CHECK_FALSE(action);
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
    REQUIRE(prefixAction);
    CHECK(prefixAction->kind == lsp::CodeActionKind::QuickFix);
    CHECK(prefixAction->isPreferred == false);
    REQUIRE(prefixAction->edit);
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
    REQUIRE(deleteAction);
    CHECK(deleteAction->kind == lsp::CodeActionKind::QuickFix);
    CHECK(deleteAction->isPreferred == false);
    REQUIRE(deleteAction->edit);
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
    REQUIRE(prefixAction);
    CHECK(prefixAction->kind == lsp::CodeActionKind::QuickFix);
    CHECK(prefixAction->isPreferred == false);
    REQUIRE(prefixAction->edit);
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
    REQUIRE(deleteAction);
    CHECK(deleteAction->kind == lsp::CodeActionKind::QuickFix);
    CHECK(deleteAction->isPreferred == false);
    REQUIRE(deleteAction->edit);
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
    REQUIRE(prefixAction);
    CHECK(prefixAction->kind == lsp::CodeActionKind::QuickFix);
    CHECK(prefixAction->isPreferred == false);
    REQUIRE(prefixAction->edit);
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
    REQUIRE(deleteAction);
    CHECK(deleteAction->kind == lsp::CodeActionKind::QuickFix);
    CHECK(deleteAction->isPreferred == false);
    REQUIRE(deleteAction->edit);
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
    REQUIRE(action);
    CHECK(action->kind == lsp::CodeActionKind::QuickFix);
    CHECK(action->isPreferred == false);
    REQUIRE(action->edit);
    auto& changes = action->edit->changes.at(uri);
    REQUIRE_EQ(changes.size(), 1);
    CHECK_EQ(changes[0].newText, "");
    CHECK_EQ(changes[0].range.start.line, 3);
    CHECK_EQ(changes[0].range.end.line, 4);
}

TEST_CASE_FIXTURE(Fixture, "remove_all_unused_code_source_action")
{
    auto uri = newDocument("test.luau", R"(
local unused1 = 1
local unused2 = 2
local function unusedFunc()
    return 3
end
print("hello")
)");

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{0, 0}, {7, 0}};
    params.context.only = {lsp::CodeActionKind::Source};

    auto result = workspace.codeAction(params, nullptr);

    auto action = findAction(result, "Remove all unused code");
    REQUIRE(action);
    CHECK(action->kind == lsp::CodeActionKind::Source);
    REQUIRE(action->edit);
    auto& changes = action->edit->changes.at(uri);
    // Should have 3 edits: unused1, unused2, and unusedFunc
    CHECK_EQ(changes.size(), 3);
}

TEST_CASE_FIXTURE(Fixture, "remove_all_unused_code_not_shown_when_no_unused")
{
    auto uri = newDocument("test.luau", R"(
local used = 1
print(used)
)");

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{0, 0}, {3, 0}};
    params.context.only = {lsp::CodeActionKind::Source};

    auto result = workspace.codeAction(params, nullptr);

    auto action = findAction(result, "Remove all unused code");
    CHECK_FALSE(action);
}

TEST_CASE_FIXTURE(Fixture, "redundant_native_attribute_fix")
{
    auto uri = newDocument("test.luau", R"(--!native
@native
local function foo()
    return 1
end
print(foo())
)");

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{1, 0}, {2, 0}};
    params.context.only = {lsp::CodeActionKind::QuickFix};

    auto result = workspace.codeAction(params, nullptr);

    auto action = findAction(result, "Remove redundant @native attribute");
    REQUIRE(action);
    CHECK(action->kind == lsp::CodeActionKind::QuickFix);
    CHECK(action->isPreferred == false);
    REQUIRE(action->edit);
    auto& changes = action->edit->changes.at(uri);
    REQUIRE_EQ(changes.size(), 1);
    CHECK_EQ(changes[0].newText, "");
    CHECK_EQ(changes[0].range.start.line, 1);
    CHECK_EQ(changes[0].range.end.line, 2);
}

TEST_CASE_FIXTURE(Fixture, "unknown_global_add_require_fix")
{
    client->globalConfig.completion.imports.stringRequires.enabled = true;

    auto moduleUri = newDocument("MyModule.luau", R"(
return {}
)");
    workspace.frontend.check(workspace.fileResolver.getModuleName(moduleUri));

    std::string source = dedent(R"(
        local x = MyModule
    )");
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{0, 10}, {0, 18}};
    params.context.only = {lsp::CodeActionKind::QuickFix};

    auto result = workspace.codeAction(params, nullptr);

    auto action = findAction(result, "Add require for 'MyModule' from \"./MyModule\"");
    REQUIRE(action);
    CHECK(action->kind == lsp::CodeActionKind::QuickFix);
    CHECK(action->isPreferred == true);
    REQUIRE(action->edit);
    auto& changes = action->edit->changes.at(uri);
    REQUIRE_EQ(changes.size(), 1);

    auto newSource = applyEdit(source, changes);
    CHECK_EQ(newSource, dedent(R"(
        local MyModule = require("./MyModule")
        local x = MyModule
    )"));
}

TEST_CASE_FIXTURE(Fixture, "unknown_global_no_fix_when_no_matching_module")
{
    auto uri = newDocument("test.luau", R"(
local x = SomeUnknownGlobal
)");

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{1, 10}, {1, 27}};
    params.context.only = {lsp::CodeActionKind::QuickFix};

    auto result = workspace.codeAction(params, nullptr);
    REQUIRE(result);
    CHECK(result->empty());
}

TEST_CASE_FIXTURE(Fixture, "unknown_global_fix_inserts_at_correct_line")
{
    client->globalConfig.completion.imports.stringRequires.enabled = true;

    auto moduleUri = newDocument("OtherModule.luau", R"(
return {}
)");
    workspace.frontend.check(workspace.fileResolver.getModuleName(moduleUri));

    auto uri = newDocument("test.luau", R"(
local foo = require("./foo.luau")
local x = OtherModule
)");

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{2, 10}, {2, 21}};
    params.context.only = {lsp::CodeActionKind::QuickFix};

    auto result = workspace.codeAction(params, nullptr);

    auto action = findAction(result, "Add require for 'OtherModule' from \"./OtherModule\"");
    REQUIRE(action);
    REQUIRE(action->edit);
    auto& changes = action->edit->changes.at(uri);
    REQUIRE_EQ(changes.size(), 1);
    CHECK(changes[0].newText.find("local OtherModule = require") != std::string::npos);
}

TEST_CASE_FIXTURE(Fixture, "unknown_global_offers_service_import_fix")
{
    std::string source = dedent(R"(
        local storage = ReplicatedStorage
    )");
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{0, 16}, {0, 33}};
    params.context.only = {lsp::CodeActionKind::QuickFix};

    auto result = workspace.codeAction(params, nullptr);

    auto action = findAction(result, "Import service 'ReplicatedStorage'");
    REQUIRE(action);
    CHECK(action->kind == lsp::CodeActionKind::QuickFix);
    CHECK(action->isPreferred == true);
    REQUIRE(action->edit);
    auto& changes = action->edit->changes.at(uri);
    REQUIRE_EQ(changes.size(), 1);

    auto newSource = applyEdit(source, changes);
    CHECK_EQ(newSource, dedent(R"(
        local ReplicatedStorage = game:GetService("ReplicatedStorage")
        local storage = ReplicatedStorage
    )"));
}

TEST_CASE_FIXTURE(Fixture, "unknown_global_offers_instance_based_require_fix")
{
    loadSourcemap(R"(
    {
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "children": [
                    {
                        "name": "Folder",
                        "className": "Folder",
                        "children": [{ "name": "MyModule", "className": "ModuleScript" }]
                    }
                ]
            },
            {
                "name": "ServerScriptService",
                "className": "ServerScriptService",
                "children": [
                    { "name": "Script", "className": "Script", "filePaths": ["Script.server.luau"] }
                ]
            }
        ]
    }
    )");

    std::string source = dedent(R"(
        local x = MyModule
    )");
    auto uri = newDocument("Script.server.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{0, 10}, {0, 18}};
    params.context.only = {lsp::CodeActionKind::QuickFix};

    auto result = workspace.codeAction(params, nullptr);

    auto action = findAction(result, "Add require for 'MyModule' from \"ReplicatedStorage.Folder.MyModule\"");
    REQUIRE(action);
    CHECK(action->kind == lsp::CodeActionKind::QuickFix);
    REQUIRE(action->edit);
    auto& changes = action->edit->changes.at(uri);
    REQUIRE_EQ(changes.size(), 2);

    auto newSource = applyEdit(source, changes);
    CHECK_EQ(newSource, dedent(R"(
        local ReplicatedStorage = game:GetService("ReplicatedStorage")
        local MyModule = require(ReplicatedStorage.Folder.MyModule)
        local x = MyModule
    )"));
}

TEST_CASE_FIXTURE(Fixture, "unknown_global_instance_require_reuses_existing_service")
{
    loadSourcemap(R"(
    {
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "children": [
                    {
                        "name": "Folder",
                        "className": "Folder",
                        "children": [{ "name": "MyModule", "className": "ModuleScript" }]
                    }
                ]
            },
            {
                "name": "ServerScriptService",
                "className": "ServerScriptService",
                "children": [
                    { "name": "Script", "className": "Script", "filePaths": ["Script.server.luau"] }
                ]
            }
        ]
    }
    )");

    std::string source = dedent(R"(
        local ReplicatedStorage = game:GetService("ReplicatedStorage")
        local x = MyModule
    )");
    auto uri = newDocument("Script.server.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{1, 10}, {1, 18}};
    params.context.only = {lsp::CodeActionKind::QuickFix};

    auto result = workspace.codeAction(params, nullptr);

    auto action = findAction(result, "Add require for 'MyModule' from \"ReplicatedStorage.Folder.MyModule\"");
    REQUIRE(action);
    REQUIRE(action->edit);
    auto& changes = action->edit->changes.at(uri);
    REQUIRE_EQ(changes.size(), 1);

    auto newSource = applyEdit(source, changes);
    CHECK_EQ(newSource, dedent(R"(
        local ReplicatedStorage = game:GetService("ReplicatedStorage")
        local MyModule = require(ReplicatedStorage.Folder.MyModule)
        local x = MyModule
    )"));
}

TEST_CASE_FIXTURE(Fixture, "add_all_missing_requires_source_action")
{
    client->globalConfig.completion.imports.stringRequires.enabled = true;

    auto moduleA = newDocument("ModuleA.luau", R"(
return {}
)");
    auto moduleB = newDocument("ModuleB.luau", R"(
return {}
)");
    workspace.frontend.check(workspace.fileResolver.getModuleName(moduleA));
    workspace.frontend.check(workspace.fileResolver.getModuleName(moduleB));

    std::string source = dedent(R"(
        local x = ModuleA
        local y = ModuleB
    )");
    auto uri = newDocument("test.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{0, 0}, {2, 0}};
    params.context.only = {lsp::CodeActionKind::Source};

    auto result = workspace.codeAction(params, nullptr);

    auto action = findAction(result, "Add all missing requires");
    REQUIRE(action);
    CHECK(action->kind == lsp::CodeActionKind::Source);
    REQUIRE(action->edit);
    auto& changes = action->edit->changes.at(uri);
    REQUIRE_EQ(changes.size(), 2);

    auto newSource = applyEdit(source, changes);
    CHECK_EQ(newSource, dedent(R"(
        local ModuleA = require("./ModuleA")
        local ModuleB = require("./ModuleB")
        local x = ModuleA
        local y = ModuleB
    )"));
}

TEST_CASE_FIXTURE(Fixture, "add_all_missing_requires_no_action_when_no_missing")
{
    auto uri = newDocument("test.luau", R"(
local x = 1
local y = 2
)");

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{0, 0}, {3, 0}};
    params.context.only = {lsp::CodeActionKind::Source};

    auto result = workspace.codeAction(params, nullptr);

    auto action = findAction(result, "Add all missing requires");
    CHECK_FALSE(action);
}

TEST_CASE_FIXTURE(Fixture, "add_all_missing_requires_with_services")
{
    loadSourcemap(R"(
    {
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "children": [
                    { "name": "ModuleA", "className": "ModuleScript" },
                    { "name": "ModuleB", "className": "ModuleScript" }
                ]
            },
            {
                "name": "ServerScriptService",
                "className": "ServerScriptService",
                "children": [
                    { "name": "Script", "className": "Script", "filePaths": ["Script.server.luau"] }
                ]
            }
        ]
    }
    )");

    std::string source = dedent(R"(
        local a = ModuleA
        local b = ModuleB
        local rs = ReplicatedStorage
    )");
    auto uri = newDocument("Script.server.luau", source);

    lsp::CodeActionParams params;
    params.textDocument.uri = uri;
    params.range = {{0, 0}, {3, 0}};
    params.context.only = {lsp::CodeActionKind::Source};

    auto result = workspace.codeAction(params, nullptr);

    auto action = findAction(result, "Add all missing requires");
    REQUIRE(action);
    REQUIRE(action->edit);
    auto& changes = action->edit->changes.at(uri);
    REQUIRE_EQ(changes.size(), 3);

    auto newSource = applyEdit(source, changes);
    CHECK_EQ(newSource, dedent(R"(
        local ReplicatedStorage = game:GetService("ReplicatedStorage")
        local ModuleA = require(ReplicatedStorage.ModuleA)
        local ModuleB = require(ReplicatedStorage.ModuleB)
        local a = ModuleA
        local b = ModuleB
        local rs = ReplicatedStorage
    )"));
}

TEST_SUITE_END();
