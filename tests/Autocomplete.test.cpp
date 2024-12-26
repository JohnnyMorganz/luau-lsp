#include "doctest.h"
#include "Fixture.h"
#include "TempDir.h"
#include "Platform/RobloxPlatform.hpp"
#include "LSP/IostreamHelpers.hpp"

std::optional<lsp::CompletionItem> getItem(const std::vector<lsp::CompletionItem>& items, const std::string& label)
{
    for (const auto& item : items)
        if (item.label == label)
            return item;
    return std::nullopt;
}

lsp::CompletionItem requireItem(const std::vector<lsp::CompletionItem>& items, const std::string& label)
{
    auto item = getItem(items, label);
    REQUIRE_MESSAGE(item.has_value(), "no item found");
    return item.value();
}

TEST_SUITE_BEGIN("Autocomplete");

TEST_CASE_FIXTURE(Fixture, "function_autocomplete_has_documentation")
{
    auto [source, marker] = sourceWithMarker(R"(
        --- This is a function documentation comment
        local function foo()
        end

        local x = |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto item = requireItem(result, "foo");

    REQUIRE(item.documentation);
    CHECK_EQ(item.documentation->kind, lsp::MarkupKind::Markdown);
    trim(item.documentation->value);
    CHECK_EQ(item.documentation->value, "This is a function documentation comment");
}

TEST_CASE_FIXTURE(Fixture, "external_module_intersected_type_table_property_has_documentation")
{
    std::string typeSource = R"(
        export type A = {
            --- Example sick number
            Hello: number
        }

        export type B = {
            --- Example sick string
            Heya: string
        } & A
    )";

    auto [source, marker] = sourceWithMarker(R"(
        local bar = require("bar.luau")
        local item: bar.B = nil
        item.|
    )");

    auto typesUri = newDocument("bar.luau", typeSource);
    workspace.checkStrict(workspace.fileResolver.getModuleName((typesUri)));

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    auto item = requireItem(result, "Hello");
    REQUIRE(item.documentation);
    CHECK_EQ(item.documentation->kind, lsp::MarkupKind::Markdown);
    trim(item.documentation->value);
    CHECK_EQ(item.documentation->value, "Example sick number");

    auto item2 = requireItem(result, "Heya");
    REQUIRE(item2.documentation);
    CHECK_EQ(item2.documentation->kind, lsp::MarkupKind::Markdown);
    trim(item2.documentation->value);
    CHECK_EQ(item2.documentation->value, "Example sick string");
}


TEST_CASE_FIXTURE(Fixture, "intersected_type_table_property_has_documentation")
{
    auto [source, marker] = sourceWithMarker(R"(
        type A = {
            --- Example sick number
            Hello: number
        }

        type B = {
            --- Example sick string
            Heya: string
        } & A

        local item: B = nil
        item.|
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    auto item = requireItem(result, "Hello");
    REQUIRE(item.documentation);
    CHECK_EQ(item.documentation->kind, lsp::MarkupKind::Markdown);
    trim(item.documentation->value);
    CHECK_EQ(item.documentation->value, "Example sick number");

    auto item2 = requireItem(result, "Heya");
    REQUIRE(item2.documentation);
    CHECK_EQ(item2.documentation->kind, lsp::MarkupKind::Markdown);
    trim(item2.documentation->value);
    CHECK_EQ(item2.documentation->value, "Example sick string");
}

TEST_CASE_FIXTURE(Fixture, "deprecated_marker_in_documentation_comment_applies_to_autocomplete_entry")
{
    auto [source, marker] = sourceWithMarker(R"(
        --- @deprecated Use `bar` instead
        local function foo()
        end

        local x = |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto item = requireItem(result, "foo");
    CHECK(item.deprecated);
}

TEST_CASE_FIXTURE(Fixture, "configure_properties_shown_when_autocompleting_index_with_colon")
{
    auto [source, marker] = sourceWithMarker(R"(
        local Foo = {}
        Foo.Value = 5

        function Foo:Bar()
        end

        local _ = Foo:|
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    client->globalConfig.completion.showPropertiesOnMethodCall = true;
    auto result = workspace.completion(params);
    CHECK(getItem(result, "Bar"));
    CHECK(getItem(result, "Value"));

    client->globalConfig.completion.showPropertiesOnMethodCall = false;
    result = workspace.completion(params);
    CHECK(getItem(result, "Bar"));
    CHECK_FALSE(getItem(result, "Value"));
}

TEST_CASE_FIXTURE(Fixture, "variable_with_a_class_type_should_not_have_class_entry_kind_1")
{
    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        local player: Instance = nil
        local x = p|
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto item = requireItem(result, "player");
    CHECK_EQ(item.kind, lsp::CompletionItemKind::Variable);
}

TEST_CASE_FIXTURE(Fixture, "variable_with_a_class_type_should_not_have_class_entry_kind_2")
{
    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        local function foo(player: Instance)
            local x = p|
        end
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto item = requireItem(result, "player");
    CHECK_EQ(item.kind, lsp::CompletionItemKind::Variable);
}

TEST_CASE_FIXTURE(Fixture, "string_completion_after_slash_should_replace_whole_string")
{
    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        local tbl = {
            ["Item/Foo"] = 1,
            ["Item/Bar"] = 2,
            ["Item/Baz"] = 3,
        }

        tbl["Item/|"]
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    std::vector<std::string> labels{"Item/Foo", "Item/Bar", "Item/Baz"};
    for (const auto& label : labels)
    {
        auto item = requireItem(result, label);
        CHECK_EQ(item.kind, lsp::CompletionItemKind::Field);
        REQUIRE(item.textEdit);
        CHECK_EQ(item.textEdit->range.start, lsp::Position{8, 13});
        CHECK_EQ(item.textEdit->range.end, lsp::Position{8, 18});
        CHECK_EQ(item.textEdit->newText, label);
    }
}

static void checkStringCompletionExists(const std::vector<lsp::CompletionItem>& items, const std::string& label)
{
    auto item = requireItem(items, label);
    CHECK_EQ(item.kind, lsp::CompletionItemKind::Constant);
}

TEST_CASE_FIXTURE(Fixture, "instance_new_contains_creatable_instances")
{
    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        Instance.new("|")
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(result.size(), 2);
    checkStringCompletionExists(result, "Part");
    checkStringCompletionExists(result, "TextLabel");
}

TEST_CASE_FIXTURE(Fixture, "get_service_contains_services")
{
    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        game:GetService("|")
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(result.size(), 3);
    checkStringCompletionExists(result, "ReplicatedStorage");
    checkStringCompletionExists(result, "ServerScriptService");
    checkStringCompletionExists(result, "Workspace");
}

TEST_CASE_FIXTURE(Fixture, "instance_is_a_contains_classnames")
{
    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        Instance.new("Part"):IsA("|")
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(result.size(), 8);
    checkStringCompletionExists(result, "Instance");
    checkStringCompletionExists(result, "Part");
    checkStringCompletionExists(result, "TextLabel");
    checkStringCompletionExists(result, "ReplicatedStorage");
    checkStringCompletionExists(result, "ServerScriptService");
    checkStringCompletionExists(result, "Workspace");
    checkStringCompletionExists(result, "ServiceProvider");
    checkStringCompletionExists(result, "DataModel");
}

TEST_CASE_FIXTURE(Fixture, "enum_is_a_contains_enum_items")
{
    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        Enum.HumanoidRigType.R6:IsA("|")
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(result.size(), 1);
    checkStringCompletionExists(result, "HumanoidRigType");
}

TEST_CASE_FIXTURE(Fixture, "get_property_changed_signal_includes_properties")
{
    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        local x = Instance.new("Part")
        x:GetPropertyChangedSignal("|")
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(result.size(), 4);
    checkStringCompletionExists(result, "Anchored");
    checkStringCompletionExists(result, "ClassName");
    checkStringCompletionExists(result, "Name");
    checkStringCompletionExists(result, "Parent");
}

TEST_CASE_FIXTURE(Fixture, "get_property_changed_signal_does_not_include_children_from_sourcemap")
{
    loadSourcemap(R"(
    {
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage"
            }
        ]
    })");

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        game:GetPropertyChangedSignal("|")
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(result.size(), 3);
    CHECK_EQ(getItem(result, "ReplicatedStorage"), std::nullopt);
    checkStringCompletionExists(result, "Name");
    checkStringCompletionExists(result, "Parent");
    checkStringCompletionExists(result, "ClassName");
}

TEST_CASE_FIXTURE(Fixture, "get_property_changed_signal_does_not_include_children_from_sourcemap_second_level_getsourcemaptype_ty")
{
    loadSourcemap(R"(
    {
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "children": [{"name": "Part", "className": "Part"}]
            }
        ]
    })");

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        game.ReplicatedStorage:GetPropertyChangedSignal("|")
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(result.size(), 3);
    CHECK_EQ(getItem(result, "Part"), std::nullopt);
    checkStringCompletionExists(result, "Name");
    checkStringCompletionExists(result, "Parent");
    checkStringCompletionExists(result, "ClassName");
}

TEST_CASE_FIXTURE(Fixture, "find_first_child_on_datamodel_contains_children")
{
    loadSourcemap(R"(
    {
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage"
            },
            {
                "name": "StandardPart",
                "className": "Part"
            }
        ]
    })");

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        game:FindFirstChild("|")
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(result.size(), 2);
    checkStringCompletionExists(result, "ReplicatedStorage");
    checkStringCompletionExists(result, "StandardPart");
}

TEST_CASE_FIXTURE(Fixture, "find_first_child_on_sourcemap_type_contains_children")
{
    loadSourcemap(R"(
    {
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "StandardPart",
                "className": "Part",
                "children": [
                    {
                        "name": "ChildA",
                        "className": "Part"
                    },
                    {
                        "name": "ChildB",
                        "className": "Part"
                    }
                ]
            }
        ]
    })");

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        game.StandardPart:FindFirstChild("|")
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(result.size(), 2);
    checkStringCompletionExists(result, "ChildA");
    checkStringCompletionExists(result, "ChildB");
}

TEST_CASE_FIXTURE(Fixture, "find_first_child_on_sourcemap_type_contains_children_second_level")
{
    loadSourcemap(R"(
    {
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "StandardPart",
                "className": "Part",
                "children": [
                    {
                        "name": "ChildA",
                        "className": "Part",
                        "children": [{"name": "GrandChildA", "className": "Part"}, {"name": "GrandChildB", "className": "Part"}]
                    }
                ]
            }
        ]
    })");

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        game.StandardPart.ChildA:FindFirstChild("|")
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(result.size(), 2);
    checkStringCompletionExists(result, "GrandChildA");
    checkStringCompletionExists(result, "GrandChildB");
}

TEST_CASE_FIXTURE(Fixture, "wait_for_child_on_datamodel_contains_children")
{
    loadSourcemap(R"(
    {
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage"
            },
            {
                "name": "StandardPart",
                "className": "Part"
            }
        ]
    })");

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        game:WaitForChild("|")
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(result.size(), 2);
    checkStringCompletionExists(result, "ReplicatedStorage");
    checkStringCompletionExists(result, "StandardPart");
}

TEST_CASE_FIXTURE(Fixture, "wait_for_child_on_sourcemap_type_contains_children")
{
    loadSourcemap(R"(
    {
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "StandardPart",
                "className": "Part",
                "children": [
                    {
                        "name": "ChildA",
                        "className": "Part"
                    },
                    {
                        "name": "ChildB",
                        "className": "Part"
                    }
                ]
            }
        ]
    })");

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        game.StandardPart:WaitForChild("|")
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(result.size(), 2);
    checkStringCompletionExists(result, "ChildA");
    checkStringCompletionExists(result, "ChildB");
}

TEST_CASE_FIXTURE(Fixture, "auto_imports_handles_multi_line_existing_requires_when_adding_new_require_before")
{
    auto source = R"(
        local _ =
            require(script.Parent.d)
    )";
    auto astRoot = parse(source);
    auto uri = newDocument("foo.luau", source);
    auto textDocument = workspace.fileResolver.getTextDocument(uri);
    REQUIRE(textDocument);

    RobloxFindImportsVisitor importsVisitor;
    importsVisitor.visit(astRoot);

    auto minimumLineNumber = computeMinimumLineNumberForRequire(importsVisitor, 0);
    auto insertedLineNumber = computeBestLineForRequire(importsVisitor, *textDocument, "script.Parent.c", minimumLineNumber);

    CHECK_EQ(insertedLineNumber, 1);
}

TEST_CASE_FIXTURE(Fixture, "auto_imports_handles_multi_line_existing_requires_when_adding_new_require_after")
{
    auto source = R"(
        local _ =
            require(script.Parent.d)
    )";
    auto astRoot = parse(source);
    auto uri = newDocument("foo.luau", source);
    auto textDocument = workspace.fileResolver.getTextDocument(uri);
    REQUIRE(textDocument);

    RobloxFindImportsVisitor importsVisitor;
    importsVisitor.visit(astRoot);

    auto minimumLineNumber = computeMinimumLineNumberForRequire(importsVisitor, 0);
    auto insertedLineNumber = computeBestLineForRequire(importsVisitor, *textDocument, "script.Parent.e", minimumLineNumber);

    CHECK_EQ(insertedLineNumber, 3);
}

static void checkFileCompletionExists(const std::vector<lsp::CompletionItem>& items, const std::string& label)
{
    auto item = requireItem(items, label);
    CHECK_EQ(item.kind, lsp::CompletionItemKind::File);
}

static void checkFolderCompletionExists(const std::vector<lsp::CompletionItem>& items, const std::string& label)
{
    auto item = requireItem(items, label);
    CHECK_EQ(item.kind, lsp::CompletionItemKind::Folder);
}

TEST_CASE_FIXTURE(Fixture, "string_require_default_shows_files_in_current_directory")
{
    TempDir t("require_default");
    t.write_child("first.luau", "return {}");
    t.write_child("second.luau", "return {}");

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        local x = require("|")
    )");

    auto uri = newDocument(t.write_child("source.luau", source), source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    REQUIRE_EQ(result.size(), 4);
    checkFolderCompletionExists(result, "..");
    checkFileCompletionExists(result, "source.luau");
    checkFileCompletionExists(result, "first.luau");
    checkFileCompletionExists(result, "second.luau");
}

TEST_CASE_FIXTURE(Fixture, "string_require_shows_files_in_current_directory")
{
    TempDir t("require_current_directory");
    t.write_child("first.luau", "return {}");
    t.write_child("second.luau", "return {}");

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        local x = require("./|")
    )");

    auto uri = newDocument(t.write_child("source.luau", source), source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    REQUIRE_EQ(result.size(), 4);
    checkFolderCompletionExists(result, "..");
    checkFileCompletionExists(result, "source.luau");
    checkFileCompletionExists(result, "first.luau");
    checkFileCompletionExists(result, "second.luau");
}

TEST_CASE_FIXTURE(Fixture, "string_require_shows_files_in_parent_directory")
{
    TempDir t("require_parents");
    t.write_child("parent_first.luau", "return {}");
    t.write_child("parent_second.luau", "return {}");

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        local x = require("../|")
    )");

    auto uri = newDocument(t.write_child("nested/source.luau", source), source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    REQUIRE_EQ(result.size(), 4);
    checkFolderCompletionExists(result, "..");
    checkFolderCompletionExists(result, "nested");
    checkFileCompletionExists(result, "parent_first.luau");
    checkFileCompletionExists(result, "parent_second.luau");
}

TEST_CASE_FIXTURE(Fixture, "string_require_shows_file_in_nested_directory")
{
    TempDir t("require_nested");
    t.write_child("sibling.luau", "return {}");
    t.write_child("nested/child.luau", "return {}");

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        local x = require("./nested/|")
    )");

    auto uri = newDocument(t.touch_child("source.luau"), source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    REQUIRE_EQ(result.size(), 2);
    checkFolderCompletionExists(result, "..");
    checkFileCompletionExists(result, "child.luau");
}

TEST_CASE_FIXTURE(Fixture, "string_require_shows_files_in_current_directory_after_entering_then_exiting_child_folder")
{
    TempDir t("require_nested");
    t.write_child("sibling.luau", "return {}");
    t.write_child("nested/child.luau", "return {}");

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        local x = require("./nested/../|")
    )");

    auto uri = newDocument(t.touch_child("source.luau"), source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    REQUIRE_EQ(result.size(), 4);
    checkFolderCompletionExists(result, "..");
    checkFolderCompletionExists(result, "nested");
    checkFileCompletionExists(result, "source.luau");
    checkFileCompletionExists(result, "sibling.luau");
}

TEST_CASE_FIXTURE(Fixture, "string_require_contains_luaurc_aliases")
{
    TempDir t("aliases");
    loadLuaurc(R"(
    {
        "aliases": {
            "Roact": "roact",
            "Fusion": "fusion"
        }
    })");

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        local x = require("|")
    )");

    auto uri = newDocument(t.touch_child("source.luau"), source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(result.size(), 4);
    checkFolderCompletionExists(result, "..");
    checkFileCompletionExists(result, "source.luau");
    requireItem(result, "@Roact");
    requireItem(result, "@Fusion");
}

TEST_CASE_FIXTURE(Fixture, "string_require_contains_file_aliases")
{
    TempDir t("file_aliases");
    client->globalConfig.require.fileAliases = {
        {"@test1", "file1.luau"},
        {"@test2", "file2.luau"},
    };

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        local x = require("|")
    )");

    auto uri = newDocument(t.touch_child("source.luau"), source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(result.size(), 4);
    checkFolderCompletionExists(result, "..");
    checkFileCompletionExists(result, "source.luau");
    checkFileCompletionExists(result, "@test1");
    checkFileCompletionExists(result, "@test2");
}

TEST_CASE_FIXTURE(Fixture, "string_require_contains_directory_aliases")
{
    TempDir t("directory_aliases");
    client->globalConfig.require.directoryAliases = {
        {"@dir1", "directory1"},
        {"@dir2", "directory2"},
    };

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        local x = require("|")
    )");

    auto uri = newDocument(t.touch_child("source.luau"), source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(result.size(), 4);
    checkFolderCompletionExists(result, "..");
    checkFileCompletionExists(result, "source.luau");
    checkFolderCompletionExists(result, "@dir1");
    checkFolderCompletionExists(result, "@dir2");
}

TEST_CASE_FIXTURE(Fixture, "string_require_doesnt_show_aliases_after_a_directory_separator_is_seen")
{
    loadLuaurc(R"(
    {
        "aliases": {
            "Roact": "roact",
            "Fusion": "fusion"
        }
    })");

    client->globalConfig.require.fileAliases = {
        {"@test1", "file1.luau"},
        {"@test2", "file2.luau"},
    };
    client->globalConfig.require.directoryAliases = {
        {"@dir1", "directory1"},
        {"@dir2", "directory2"},
    };

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        local x = require("test/|")
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "string_require_shows_files_under_a_directory_alias")
{
    TempDir t("lune");
    t.write_child("process.luau", "return {}");
    t.write_child("net.luau", "return {}");

    client->globalConfig.require.directoryAliases = {
        {"@lune", t.path()},
    };

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        local x = require("@lune/|")
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    REQUIRE_EQ(result.size(), 3);
    checkFolderCompletionExists(result, "..");
    checkFileCompletionExists(result, "process.luau");
    checkFileCompletionExists(result, "net.luau");
}

TEST_CASE_FIXTURE(Fixture, "string_require_shows_files_under_a_luaurc_directory_alias")
{
    TempDir t("lune");
    t.write_child("process.luau", "return {}");
    t.write_child("net.luau", "return {}");

    std::string luaurc = R"(
    {
        "aliases": {
            "lune": "{PATH}",
        }
    })";
    replace(luaurc, "{PATH}", t.path());
    loadLuaurc(luaurc);

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        local x = require("@lune/|")
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    REQUIRE_EQ(result.size(), 3);
    checkFolderCompletionExists(result, "..");
    checkFileCompletionExists(result, "process.luau");
    checkFileCompletionExists(result, "net.luau");
}

TEST_SUITE_END();
