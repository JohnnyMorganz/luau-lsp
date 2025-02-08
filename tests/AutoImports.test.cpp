#include "doctest.h"
#include "Fixture.h"
#include "TempDir.h"
#include "Platform/RobloxPlatform.hpp"
#include "LSP/IostreamHelpers.hpp"

static std::optional<lsp::CompletionItem> getItem(const std::vector<lsp::CompletionItem>& items, const std::string& label)
{
    for (const auto& item : items)
        if (item.label == label)
            return item;

    return std::nullopt;
}

static std::vector<lsp::CompletionItem> filterAutoImports(const std::vector<lsp::CompletionItem>& items, const std::string& moduleName = "")
{
    std::vector<lsp::CompletionItem> results;

    for (const auto& item : items)
    {
        if (item.kind == lsp::CompletionItemKind::Module)
            if (moduleName.empty() || item.label == moduleName)
                results.emplace_back(item);
    }

    return results;
}

TEST_SUITE_BEGIN("AutoImports");

TEST_CASE_FIXTURE(Fixture, "services_show_up_in_auto_import")
{
    client->globalConfig.completion.imports.enabled = true;
    auto [source, marker] = sourceWithMarker(R"(
        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    auto serviceImport = getItem(result, "ReplicatedStorage");
    REQUIRE(serviceImport);

    CHECK_EQ(serviceImport->label, "ReplicatedStorage");
    CHECK_EQ(serviceImport->insertText, "ReplicatedStorage");
    CHECK_EQ(serviceImport->detail, "Auto-import");
    CHECK_EQ(serviceImport->kind, lsp::CompletionItemKind::Class);
    REQUIRE_EQ(serviceImport->additionalTextEdits.size(), 1);
    CHECK_EQ(serviceImport->additionalTextEdits[0].newText, "local ReplicatedStorage = game:GetService(\"ReplicatedStorage\")\n");
    CHECK_EQ(serviceImport->additionalTextEdits[0].range, lsp::Range{{0, 0}, {0, 0}});

    CHECK(getItem(result, "ServerScriptService"));
    CHECK(getItem(result, "Workspace"));
}

TEST_CASE_FIXTURE(Fixture, "services_do_not_show_up_in_autocomplete_if_imports_is_disabled")
{
    client->globalConfig.completion.imports.enabled = false;
    auto [source, marker] = sourceWithMarker(R"(
        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    CHECK_EQ(getItem(result, "ReplicatedStorage"), std::nullopt);
}

TEST_CASE_FIXTURE(Fixture, "services_do_not_show_up_in_autocomplete_if_suggest_services_is_disabled")
{
    client->globalConfig.completion.imports.enabled = true;
    client->globalConfig.completion.imports.suggestServices = false;
    auto [source, marker] = sourceWithMarker(R"(
        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    CHECK_EQ(getItem(result, "ReplicatedStorage"), std::nullopt);
}

TEST_CASE_FIXTURE(Fixture, "service_does_not_show_up_in_autocomplete_if_already_exists")
{
    client->globalConfig.completion.imports.enabled = true;
    auto [source, marker] = sourceWithMarker(R"(
        local ReplicatedStorage = game:GetService("ReplicatedStorage")
        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    // item will exist as variable import, but it shouldn't be an auto import
    auto item = getItem(result, "ReplicatedStorage");
    CHECK_NE(item->detail, "Auto-import");
    CHECK_EQ(item->additionalTextEdits.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "service_auto_imports_are_inserted_alphabetically")
{
    client->globalConfig.completion.imports.enabled = true;
    auto [source, marker] = sourceWithMarker(R"(
        local ServerScriptService = game:GetService("ServerScriptService")
        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    auto beforeImport = getItem(result, "ReplicatedStorage");
    REQUIRE_EQ(beforeImport->additionalTextEdits.size(), 1);
    CHECK_EQ(beforeImport->additionalTextEdits[0].range, lsp::Range{{1, 0}, {1, 0}});

    auto afterImport = getItem(result, "Workspace");
    REQUIRE_EQ(afterImport->additionalTextEdits.size(), 1);
    CHECK_EQ(afterImport->additionalTextEdits[0].range, lsp::Range{{2, 0}, {2, 0}});
}

TEST_CASE_FIXTURE(Fixture, "service_auto_imports_are_inserted_after_hot_comments")
{
    client->globalConfig.completion.imports.enabled = true;
    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    auto import = getItem(result, "ReplicatedStorage");
    REQUIRE(import);
    REQUIRE_EQ(import->additionalTextEdits.size(), 1);
    CHECK_EQ(import->additionalTextEdits[0].range, lsp::Range{{2, 0}, {2, 0}});
}

TEST_CASE_FIXTURE(Fixture, "module_script_shows_up_in_auto_imports")
{
    client->globalConfig.completion.imports.enabled = true;
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
                        "children": [{ "name": "Module", "className": "ModuleScript" }]
                    }
                ]
            }
        ]
    }
    )");

    auto [source, marker] = sourceWithMarker(R"(
        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result, "Module");

    REQUIRE_EQ(imports.size(), 1);
    CHECK_EQ(imports[0].label, "Module");
    CHECK_EQ(imports[0].insertText, "Module");
    CHECK_EQ(imports[0].kind, lsp::CompletionItemKind::Module);

    REQUIRE_EQ(imports[0].additionalTextEdits.size(), 2);
    CHECK_EQ(imports[0].additionalTextEdits[0].newText, "local ReplicatedStorage = game:GetService(\"ReplicatedStorage\")\n");
    CHECK_EQ(imports[0].additionalTextEdits[0].range, lsp::Range{{0, 0}, {0, 0}});
    CHECK_EQ(imports[0].additionalTextEdits[1].newText, "local Module = require(ReplicatedStorage.Folder.Module)\n");
    CHECK_EQ(imports[0].additionalTextEdits[1].range, lsp::Range{{0, 0}, {0, 0}});
}

TEST_CASE_FIXTURE(Fixture, "module_script_does_not_show_up_in_autocomplete_if_imports_is_disabled")
{
    client->globalConfig.completion.imports.enabled = false;
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
                        "children": [{ "name": "Module", "className": "ModuleScript" }]
                    }
                ]
            }
        ]
    }
    )");

    auto [source, marker] = sourceWithMarker(R"(
        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(filterAutoImports(result, "Module").size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "module_script_does_not_show_up_in_autocomplete_if_require_imports_is_disabled")
{
    client->globalConfig.completion.imports.enabled = true;
    client->globalConfig.completion.imports.suggestRequires = false;
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
                        "children": [{ "name": "Module", "className": "ModuleScript" }]
                    }
                ]
            }
        ]
    }
    )");

    auto [source, marker] = sourceWithMarker(R"(
        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);

    CHECK_EQ(filterAutoImports(result, "Module").size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "auto_import_reuses_service_if_already_defined")
{
    client->globalConfig.completion.imports.enabled = true;
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
                        "children": [{ "name": "Module", "className": "ModuleScript" }]
                    }
                ]
            }
        ]
    }
    )");

    auto [source, marker] = sourceWithMarker(R"(
        local ReplicatedStorage = game:GetService("ReplicatedStorage")
        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result, "Module");

    REQUIRE_EQ(imports.size(), 1);
    REQUIRE_EQ(imports[0].additionalTextEdits.size(), 1);
    CHECK_EQ(imports[0].additionalTextEdits[0].newText, "local Module = require(ReplicatedStorage.Folder.Module)\n");
    CHECK_EQ(imports[0].additionalTextEdits[0].range, lsp::Range{{2, 0}, {2, 0}});
}

TEST_CASE_FIXTURE(Fixture, "auto_import_separates_new_service_and_require_with_line")
{
    client->globalConfig.completion.imports.enabled = true;
    client->globalConfig.completion.imports.separateGroupsWithLine = true;
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
                        "children": [{ "name": "Module", "className": "ModuleScript" }]
                    }
                ]
            }
        ]
    }
    )");

    auto [source, marker] = sourceWithMarker(R"(
        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result, "Module");

    REQUIRE_EQ(imports.size(), 1);
    REQUIRE_EQ(imports[0].additionalTextEdits.size(), 2);
    CHECK_EQ(imports[0].additionalTextEdits[0].newText, "local ReplicatedStorage = game:GetService(\"ReplicatedStorage\")\n\n");
    CHECK_EQ(imports[0].additionalTextEdits[0].range, lsp::Range{{0, 0}, {0, 0}});
    CHECK_EQ(imports[0].additionalTextEdits[1].newText, "local Module = require(ReplicatedStorage.Folder.Module)\n");
    CHECK_EQ(imports[0].additionalTextEdits[1].range, lsp::Range{{0, 0}, {0, 0}});
}

TEST_CASE_FIXTURE(Fixture, "auto_import_separates_existing_service_and_new_require_with_line")
{
    client->globalConfig.completion.imports.enabled = true;
    client->globalConfig.completion.imports.separateGroupsWithLine = true;
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
                        "children": [{ "name": "Module", "className": "ModuleScript" }]
                    }
                ]
            }
        ]
    }
    )");

    auto [source, marker] = sourceWithMarker(R"(
        local ReplicatedStorage = game:GetService("ReplicatedStorage")
        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result, "Module");

    REQUIRE_EQ(imports.size(), 1);
    REQUIRE_EQ(imports[0].additionalTextEdits.size(), 1);
    CHECK_EQ(imports[0].additionalTextEdits[0].newText, "\nlocal Module = require(ReplicatedStorage.Folder.Module)\n");
    CHECK_EQ(imports[0].additionalTextEdits[0].range, lsp::Range{{2, 0}, {2, 0}});
}

TEST_CASE_FIXTURE(Fixture, "auto_imports_will_prefer_relative_over_absolute_import_for_sibling_modules")
{
    client->globalConfig.completion.imports.enabled = true;
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
                        "name": "Module",
                        "className": "ModuleScript",
                        "filePaths": ["source.luau"]
                    },
                    {
                        "name": "SiblingModule",
                        "className": "ModuleScript"
                    }
                ]
            }
        ]
    }
    )");

    auto [source, marker] = sourceWithMarker(R"(
        |
    )");

    auto uri = newDocument("source.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result, "SiblingModule");

    REQUIRE_EQ(imports.size(), 1);
    CHECK_EQ(imports[0].label, "SiblingModule");
    REQUIRE_EQ(imports[0].additionalTextEdits.size(), 1);
    CHECK_EQ(imports[0].additionalTextEdits[0].newText, "local SiblingModule = require(script.Parent.SiblingModule)\n");
}

TEST_CASE_FIXTURE(Fixture, "auto_imports_will_force_absolute_import_depending_on_setting")
{
    client->globalConfig.completion.imports.enabled = true;
    client->globalConfig.completion.imports.requireStyle = ImportRequireStyle::AlwaysAbsolute;
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
                        "name": "Module",
                        "className": "ModuleScript",
                        "filePaths": ["source.luau"]
                    },
                    {
                        "name": "SiblingModule",
                        "className": "ModuleScript"
                    }
                ]
            }
        ]
    }
    )");

    auto [source, marker] = sourceWithMarker(R"(
        |
    )");

    auto uri = newDocument("source.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result, "SiblingModule");

    REQUIRE_EQ(imports.size(), 1);
    CHECK_EQ(imports[0].label, "SiblingModule");
    REQUIRE_EQ(imports[0].additionalTextEdits.size(), 2);
    CHECK_EQ(imports[0].additionalTextEdits[0].newText, "local ReplicatedStorage = game:GetService(\"ReplicatedStorage\")\n");
    CHECK_EQ(imports[0].additionalTextEdits[1].newText, "local SiblingModule = require(ReplicatedStorage.SiblingModule)\n");
}

TEST_CASE_FIXTURE(Fixture, "auto_imports_will_prefer_absolute_import_over_relative_import_for_further_away_modules")
{
    client->globalConfig.completion.imports.enabled = true;
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
                        "name": "Module",
                        "className": "ModuleScript",
                        "filePaths": ["source.luau"]
                    },
                    {
                        "name": "Folder",
                        "className": "Folder",
                        "children": [
                            { "name": "OtherModule", "className": "ModuleScript" }
                        ]
                    }
                ]
            }
        ]
    }
    )");

    auto [source, marker] = sourceWithMarker(R"(
        |
    )");

    auto uri = newDocument("source.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result, "OtherModule");

    REQUIRE_EQ(imports.size(), 1);
    CHECK_EQ(imports[0].label, "OtherModule");
    REQUIRE_EQ(imports[0].additionalTextEdits.size(), 2);
    CHECK_EQ(imports[0].additionalTextEdits[0].newText, "local ReplicatedStorage = game:GetService(\"ReplicatedStorage\")\n");
    CHECK_EQ(imports[0].additionalTextEdits[1].newText, "local OtherModule = require(ReplicatedStorage.Folder.OtherModule)\n");
}

TEST_CASE_FIXTURE(Fixture, "auto_imports_will_force_relative_import_depending_on_setting")
{
    client->globalConfig.completion.imports.enabled = true;
    client->globalConfig.completion.imports.requireStyle = ImportRequireStyle::AlwaysRelative;
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
                        "name": "Module",
                        "className": "ModuleScript",
                        "filePaths": ["source.luau"]
                    },
                    {
                        "name": "Folder",
                        "className": "Folder",
                        "children": [
                            { "name": "OtherModule", "className": "ModuleScript" }
                        ]
                    }
                ]
            }
        ]
    }
    )");

    auto [source, marker] = sourceWithMarker(R"(
        |
    )");

    auto uri = newDocument("source.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result, "OtherModule");

    REQUIRE_EQ(imports.size(), 1);
    CHECK_EQ(imports[0].label, "OtherModule");
    REQUIRE_EQ(imports[0].additionalTextEdits.size(), 1);
    CHECK_EQ(imports[0].additionalTextEdits[0].newText, "local OtherModule = require(script.Parent.Folder.OtherModule)\n");
}

TEST_CASE_FIXTURE(Fixture, "auto_imports_of_modules_show_path_name")
{
    client->globalConfig.completion.imports.enabled = true;
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
                        "name": "Folder1",
                        "className": "Folder",
                        "children": [{ "name": "Module", "className": "ModuleScript" }]
                    },
                    {
                        "name": "Folder2",
                        "className": "Folder",
                        "children": [{ "name": "Module", "className": "ModuleScript" }]
                    }
                ]
            }
        ]
    }
    )");

    auto [source, marker] = sourceWithMarker(R"(
        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result, "Module");

    std::sort(imports.begin(), imports.end(),
        [](const auto& a, const auto& b)
        {
            return a.detail < b.detail;
        });

    REQUIRE_EQ(imports.size(), 2);
    CHECK_EQ(imports[0].detail, "ReplicatedStorage.Folder1.Module");
    CHECK_EQ(imports[1].detail, "ReplicatedStorage.Folder2.Module");
    CHECK_EQ(imports[0].labelDetails->description, "ReplicatedStorage.Folder1.Module");
    CHECK_EQ(imports[1].labelDetails->description, "ReplicatedStorage.Folder2.Module");
}

TEST_SUITE_END();
