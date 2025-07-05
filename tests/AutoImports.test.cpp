#include "doctest.h"
#include "Fixture.h"
#include "Platform/RobloxPlatform.hpp"
#include "LSP/IostreamHelpers.hpp"
#include "Platform/StringRequireAutoImporter.hpp"
#include "TempDir.h"

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

    std::sort(results.begin(), results.end(),
        [](const auto& a, const auto& b)
        {
            return a.detail < b.detail;
        });

    return results;
}

TEST_SUITE_BEGIN("AutoImports");

TEST_CASE("make_valid_variable_names")
{
    using namespace Luau::LanguageServer::AutoImports;
    CHECK_EQ(makeValidVariableName("react spring"), "react_spring");
    CHECK_EQ(makeValidVariableName("react-spring"), "react_spring");
    CHECK_EQ(makeValidVariableName("react@spring"), "react_spring");
}

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

    REQUIRE_EQ(imports.size(), 2);
    CHECK_EQ(imports[0].detail, "ReplicatedStorage.Folder1.Module");
    CHECK_EQ(imports[1].detail, "ReplicatedStorage.Folder2.Module");
    CHECK_EQ(imports[0].labelDetails->description, "ReplicatedStorage.Folder1.Module");
    CHECK_EQ(imports[1].labelDetails->description, "ReplicatedStorage.Folder2.Module");
}

TEST_CASE_FIXTURE(Fixture, "instance_auto_imports_creates_valid_identifier")
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
                        "name": "react-spring",
                        "className": "ModuleScript",
                        "filePaths": ["module.luau"]
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
    auto imports = filterAutoImports(result);

    REQUIRE_EQ(imports.size(), 1);
    CHECK_EQ(imports[0].label, "react_spring");
    REQUIRE_EQ(imports[0].additionalTextEdits.size(), 2);
    CHECK_EQ(imports[0].additionalTextEdits[1].newText, "local react_spring = require(ReplicatedStorage[\"react-spring\"])\n");
}

TEST_CASE_FIXTURE(Fixture, "auto_imports_handles_children_of_module")
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
                        "filePaths": ["init.luau"],
                        "children": [
                            {
                                "name": "ChildModule",
                                "className": "ModuleScript"
                            }
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

    auto uri = newDocument("init.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result, "ChildModule");

    REQUIRE_EQ(imports.size(), 1);
    CHECK_EQ(imports[0].label, "ChildModule");
    REQUIRE_EQ(imports[0].additionalTextEdits.size(), 1);
    CHECK_EQ(imports[0].additionalTextEdits[0].newText, "local ChildModule = require(script.ChildModule)\n");
}

TEST_CASE_FIXTURE(Fixture, "auto_imports_handles_descendant_of_module")
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
                        "filePaths": ["init.luau"],
                        "children": [
                            {
                                "name": "Folder",
                                "className": "Folder",
                                "children": [
                                    {
                                        "name": "DescendantModule",
                                        "className": "ModuleScript"
                                    }
                                ]
                            }
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

    auto uri = newDocument("init.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result, "DescendantModule");

    REQUIRE_EQ(imports.size(), 1);
    CHECK_EQ(imports[0].label, "DescendantModule");
    REQUIRE_EQ(imports[0].additionalTextEdits.size(), 1);
    CHECK_EQ(imports[0].additionalTextEdits[0].newText, "local DescendantModule = require(script.Folder.DescendantModule)\n");
}

TEST_CASE_FIXTURE(Fixture, "auto_imports_handles_parent_of_module")
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
                        "filePaths": ["init.luau"],
                        "children": [
                            {
                                "name": "ChildModule",
                                "className": "ModuleScript",
                                "filePaths": ["source.luau"]
                            }
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
    auto imports = filterAutoImports(result, "Module");

    REQUIRE_EQ(imports.size(), 1);
    CHECK_EQ(imports[0].label, "Module");
    REQUIRE_EQ(imports[0].additionalTextEdits.size(), 1);
    CHECK_EQ(imports[0].additionalTextEdits[0].newText, "local Module = require(script.Parent)\n");
}

TEST_CASE_FIXTURE(Fixture, "auto_imports_handles_ancestor_of_module")
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
                        "filePaths": ["init.luau"],
                        "children": [
                            {
                                "name": "Folder",
                                "className": "Folder",
                                "children": [
                                    {
                                        "name": "DescendantModule",
                                        "className": "ModuleScript",
                                        "filePaths": ["source.luau"]
                                    }
                                ]
                            }
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
    auto imports = filterAutoImports(result, "Module");

    REQUIRE_EQ(imports.size(), 1);
    CHECK_EQ(imports[0].label, "Module");
    REQUIRE_EQ(imports[0].additionalTextEdits.size(), 1);
    CHECK_EQ(imports[0].additionalTextEdits[0].newText, "local Module = require(script.Parent.Parent)\n");
}

using namespace Luau::LanguageServer::AutoImports;

static StringRequireAutoImporterContext createContext(Fixture* fixture, const Uri& uri, FindImportsVisitor* importsVisitor)
{
    auto moduleName = fixture->workspace.fileResolver.getModuleName(uri);
    auto sourceModule = fixture->workspace.frontend.getSourceModule(moduleName);
    auto textDocument = fixture->workspace.fileResolver.getTextDocument(uri);

    // Need to pass the visitor in so that it isn't destroyed after function ends
    importsVisitor->visit(sourceModule->root);

    StringRequireAutoImporterContext ctx{
        moduleName,
        Luau::NotNull(textDocument),
        Luau::NotNull(&fixture->workspace.frontend),
        Luau::NotNull(&fixture->workspace),
        Luau::NotNull(&fixture->client->globalConfig.completion.imports),
        0,
        Luau::NotNull(importsVisitor),
    };

    return ctx;
}

TEST_CASE_FIXTURE(Fixture, "module_name_processed_for_label")
{
    CHECK_EQ(requireNameFromModuleName("foo.luau"), "foo");
    CHECK_EQ(requireNameFromModuleName("bar.lua"), "bar");
    CHECK_EQ(requireNameFromModuleName("a/b/c/baz.luau"), "baz");
}

TEST_CASE_FIXTURE(Fixture, "string_require_label_is_file_name")
{
    newDocument("foo.luau", "");
    newDocument("bar.lua", "");

    FindImportsVisitor visitor;
    auto user = newDocument("user.luau", "");
    auto ctx = createContext(this, user, &visitor);

    std::vector<lsp::CompletionItem> items;
    suggestStringRequires(ctx, items);

    CHECK_EQ(items.size(), 2);

    auto firstItem = getItem(items, "foo");
    REQUIRE(firstItem);
    CHECK_EQ(firstItem->label, "foo");
    CHECK_EQ(firstItem->additionalTextEdits.size(), 1);
    CHECK_EQ(firstItem->additionalTextEdits[0].newText, "local foo = require(\"./foo\")\n");

    auto secondItem = getItem(items, "bar");
    CHECK_EQ(secondItem->label, "bar");
    CHECK_EQ(secondItem->additionalTextEdits.size(), 1);
    CHECK_EQ(secondItem->additionalTextEdits[0].newText, "local bar = require(\"./bar\")\n");
    CHECK(getItem(items, "foo"));
}

TEST_CASE_FIXTURE(Fixture, "string_requires_does_not_include_source_module")
{
    FindImportsVisitor visitor;
    auto user = newDocument("user.luau", "");
    auto ctx = createContext(this, user, &visitor);

    std::vector<lsp::CompletionItem> items;
    suggestStringRequires(ctx, items);

    CHECK_EQ(items.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "string_require_does_not_include_modules_matching_globs")
{
    newDocument("library.luau", "");
    newDocument("library.spec.luau", "");
    newDocument("Packages/React.luau", "");
    newDocument("Packages/_Index/ReactLib.luau", "");

    client->globalConfig.completion.imports.ignoreGlobs = {"*.spec.luau", "**/_Index/**"};

    FindImportsVisitor visitor;
    auto user = newDocument("user.luau", "");
    auto ctx = createContext(this, user, &visitor);

    std::vector<lsp::CompletionItem> items;
    suggestStringRequires(ctx, items);

    CHECK_EQ(items.size(), 2);
    CHECK(getItem(items, "library"));
    CHECK(getItem(items, "React"));

    CHECK_FALSE(getItem(items, "library.spec"));
    CHECK_FALSE(getItem(items, "ReactLib"));
}

TEST_CASE_FIXTURE(Fixture, "string_require_does_not_include_modules_that_are_already_required")
{
    newDocument("library.luau", "");
    newDocument("Packages/React.luau", "");

    FindImportsVisitor visitor;
    auto user = newDocument("user.luau", R"(
        local React = require("./Packages/React")
    )");
    auto ctx = createContext(this, user, &visitor);

    std::vector<lsp::CompletionItem> items;
    suggestStringRequires(ctx, items);

    CHECK_EQ(items.size(), 1);
    CHECK(getItem(items, "library"));
    CHECK_FALSE(getItem(items, "React"));
}

TEST_CASE_FIXTURE(Fixture, "string_require_uses_aliases")
{
    AliasMap aliases{""};
    aliases["Packages"] = {Uri::file("Packages").fsPath(), "", "Packages"};
    auto style = ImportRequireStyle::Auto;

    auto from = Uri::file("project/user.luau");
    CHECK_EQ(computeRequirePath(from, Uri::file("Packages/React.luau"), aliases, style).first, "@Packages/React");
}

TEST_CASE_FIXTURE(Fixture, "dont_use_aliases_when_always_relative_specified")
{
    AliasMap aliases{""};
    aliases["Packages"] = {Uri::file("Packages").fsPath(), "", "Packages"};
    auto style = ImportRequireStyle::AlwaysRelative;

    auto from = Uri::file("project/user.luau");
    CHECK_EQ(computeRequirePath(from, Uri::file("Packages/React.luau"), aliases, style).first, "../Packages/React");
}

TEST_CASE_FIXTURE(Fixture, "always_use_possible_aliases_when_always_absolute_specified")
{
    AliasMap aliases{""};
    aliases["project"] = {Uri::file("project").fsPath(), "", "project"};
    auto style = ImportRequireStyle::AlwaysAbsolute;

    auto from = Uri::file("project/user.luau");
    CHECK_EQ(computeRequirePath(from, Uri::file("project/sibling.luau"), aliases, style).first, "@project/sibling");
    CHECK_EQ(computeRequirePath(from, Uri::file("project/directory/child.luau"), aliases, style).first, "@project/directory/child");
    CHECK_EQ(computeRequirePath(from, Uri::file("other/lib.luau"), aliases, style).first, "../other/lib");
}

TEST_CASE_FIXTURE(Fixture, "string_require_compute_best_alias")
{
    AliasMap aliases{""};
    aliases["project"] = {Uri::file("project").fsPath(), "", "Project"};
    aliases["packages"] = {Uri::file("packages").fsPath(), "", "Packages"};
    aliases["nestedproject"] = {Uri::file("project/nested").fsPath(), "", "NestedProject"};

    CHECK_EQ(computeBestAliasedPath(Uri::file("project/user.luau"), aliases), "@Project/user");
    CHECK_EQ(computeBestAliasedPath(Uri::file("packages/React.luau"), aliases), "@Packages/React");
    CHECK_EQ(computeBestAliasedPath(Uri::file("packages/nested/library/React.luau"), aliases), "@Packages/nested/library/React");
    CHECK_EQ(computeBestAliasedPath(Uri::file("project/nested/lib.luau"), aliases), "@NestedProject/lib");
    CHECK_EQ(computeBestAliasedPath(Uri::file("random/test.luau"), aliases), std::nullopt);
}

TEST_CASE_FIXTURE(Fixture, "string_require_uses_best_alias_from_luaurc")
{
    loadLuaurc(R"(
    {
        "aliases": {
            "ReplicatedStorage": "src/shared",
            "Modules": "src/shared/Modules",
        }
    })");

    client->globalConfig.completion.imports.enabled = true;

    // HACK: Fixture is loaded for RobloxPlatform
    client->globalConfig.platform.type = LSPPlatformConfig::Standard;
    workspace.appliedFirstTimeConfiguration = false;
    workspace.setupWithConfiguration(client->globalConfig);

    newDocument("src/shared/Modules/Module.luau", "return {}");

    auto [source, marker] = sourceWithMarker(R"(
        |
    )");

    auto uri = newDocument("src/client/file.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result);

    REQUIRE_EQ(imports.size(), 1);
    REQUIRE_EQ(imports[0].additionalTextEdits.size(), 1);
    CHECK_EQ(imports[0].additionalTextEdits[0].newText, "local Module = require(\"@Modules/Module\")\n");
}

TEST_CASE_FIXTURE(Fixture, "string_require_resolves_correctly_for_init_luau_file")
{
    AliasMap aliases{""};
    auto style = ImportRequireStyle::Auto;

    auto from = Uri::file("project/code/init.luau");
    CHECK_EQ(computeRequirePath(from, Uri::file("project/code/sibling.luau"), aliases, style).first, "@self/sibling");
    CHECK_EQ(computeRequirePath(from, Uri::file("project/file.luau"), aliases, style).first, "./file");
}

TEST_CASE_FIXTURE(Fixture, "string_require_resolves_to_directory_that_contains_init_luau_file")
{
    AliasMap aliases{""};
    auto style = ImportRequireStyle::Auto;

    auto from = Uri::file("project/file.luau");
    auto to = Uri::file("project/code/init.luau");

    CHECK_EQ(requireNameFromModuleName(workspace.fileResolver.getModuleName(to)), "code");
    CHECK_EQ(computeRequirePath(from, to, aliases, style).first, "./code");
}

TEST_CASE_FIXTURE(Fixture, "string_require_inserts_at_top_of_file")
{
    client->globalConfig.completion.imports.enabled = true;

    // HACK: Fixture is loaded for RobloxPlatform
    client->globalConfig.platform.type = LSPPlatformConfig::Standard;
    workspace.appliedFirstTimeConfiguration = false;
    workspace.setupWithConfiguration(client->globalConfig);

    newDocument("library.luau", "");

    auto [source, marker] = sourceWithMarker(R"(

        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result);

    REQUIRE_EQ(imports.size(), 1);
    REQUIRE_EQ(imports[0].additionalTextEdits.size(), 1);
    CHECK_EQ(imports[0].additionalTextEdits[0].range.start.line, 0);
}

TEST_CASE_FIXTURE(Fixture, "string_require_inserts_after_hot_comments")
{
    client->globalConfig.completion.imports.enabled = true;

    // HACK: Fixture is loaded for RobloxPlatform
    client->globalConfig.platform.type = LSPPlatformConfig::Standard;
    workspace.appliedFirstTimeConfiguration = false;
    workspace.setupWithConfiguration(client->globalConfig);

    newDocument("library.luau", "");

    auto [source, marker] = sourceWithMarker(R"(
        --!strict

        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result);

    REQUIRE_EQ(imports.size(), 1);
    REQUIRE_EQ(imports[0].additionalTextEdits.size(), 1);
    CHECK_EQ(imports[0].additionalTextEdits[0].range.start.line, 2);
}

TEST_CASE_FIXTURE(Fixture, "string_require_inserts_after_hot_comments_2")
{
    client->globalConfig.completion.imports.enabled = true;

    // HACK: Fixture is loaded for RobloxPlatform
    client->globalConfig.platform.type = LSPPlatformConfig::Standard;
    workspace.appliedFirstTimeConfiguration = false;
    workspace.setupWithConfiguration(client->globalConfig);

    newDocument("library.luau", "");

    auto [source, marker] = sourceWithMarker(R"(
        --!strict
        --!optimize 2

        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result);

    REQUIRE_EQ(imports.size(), 1);
    REQUIRE_EQ(imports[0].additionalTextEdits.size(), 1);
    CHECK_EQ(imports[0].additionalTextEdits[0].range.start.line, 3);
}

TEST_CASE_FIXTURE(Fixture, "string_requires_are_inserted_lexicographically_1")
{
    newDocument("library.luau", "");

    FindImportsVisitor visitor;
    auto user = newDocument("user.luau", R"(
        local zebra = require("./zebra")
    )");
    auto ctx = createContext(this, user, &visitor);

    std::vector<lsp::CompletionItem> items;
    suggestStringRequires(ctx, items);

    CHECK_EQ(items.size(), 1);
    auto item = getItem(items, "library");
    REQUIRE(item);
    REQUIRE_EQ(item->additionalTextEdits.size(), 1);
    CHECK_EQ(item->additionalTextEdits[0].range.start.line, 1);
}

TEST_CASE_FIXTURE(Fixture, "string_requires_are_inserted_lexicographically_2")
{
    newDocument("library.luau", "");

    FindImportsVisitor visitor;
    auto user = newDocument("user.luau", R"(
        local alphabet = require("./alphabet")
    )");
    auto ctx = createContext(this, user, &visitor);

    std::vector<lsp::CompletionItem> items;
    suggestStringRequires(ctx, items);

    CHECK_EQ(items.size(), 1);
    auto item = getItem(items, "library");
    REQUIRE(item);
    REQUIRE_EQ(item->additionalTextEdits.size(), 1);
    CHECK_EQ(item->additionalTextEdits[0].range.start.line, 2);
}

TEST_CASE_FIXTURE(Fixture, "string_requires_are_inserted_lexicographically_3")
{
    newDocument("library.luau", "");

    FindImportsVisitor visitor;
    auto user = newDocument("user.luau", R"(
        local alphabet = require("./alphabet")
        local book = require("./book")
        local zebra = require("./zebra")
    )");
    auto ctx = createContext(this, user, &visitor);

    std::vector<lsp::CompletionItem> items;
    suggestStringRequires(ctx, items);

    CHECK_EQ(items.size(), 1);
    auto item = getItem(items, "library");
    REQUIRE(item);
    REQUIRE_EQ(item->additionalTextEdits.size(), 1);
    CHECK_EQ(item->additionalTextEdits[0].range.start.line, 3);
}

TEST_CASE_FIXTURE(Fixture, "relative_requires_are_inserted_after_aliases")
{
    auto astRoot = parse(R"(
        local React = require("@Packages/React")
    )");

    FindImportsVisitor importsVisitor;
    importsVisitor.visit(astRoot);

    auto insertedLineNumber = computeBestLineForRequire(importsVisitor, TextDocument({}, {}, {}, {}), "./alphabet", 0);

    CHECK_EQ(insertedLineNumber, 2);
}

TEST_CASE_FIXTURE(Fixture, "aliased_requires_are_inserted_before_relative_requires")
{
    auto astRoot = parse(R"(
        local alphabet = require("./alphabet")
    )");

    FindImportsVisitor importsVisitor;
    importsVisitor.visit(astRoot);

    auto insertedLineNumber = computeBestLineForRequire(importsVisitor, TextDocument({}, {}, {}, {}), "@Packages/React", 0);

    CHECK_EQ(insertedLineNumber, 0);
}

TEST_CASE_FIXTURE(Fixture, "string_require_imports_work_on_roblox_platform")
{
    client->globalConfig.completion.imports.enabled = true;
    client->globalConfig.completion.imports.stringRequires.enabled = true;

    newDocument("library.luau", "");

    auto [source, marker] = sourceWithMarker(R"(
        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result);

    REQUIRE_EQ(imports.size(), 1);
    auto item = getItem(imports, "library");
    REQUIRE(item);
    REQUIRE_EQ(item->additionalTextEdits.size(), 1);
    CHECK_EQ(item->additionalTextEdits[0].newText, "local library = require(\"./library\")\n");
}

TEST_CASE_FIXTURE(Fixture, "string_requires_are_inserted_after_services")
{
    client->globalConfig.completion.imports.enabled = true;
    client->globalConfig.completion.imports.stringRequires.enabled = true;

    newDocument("alphabet.luau", "");

    auto [source, marker] = sourceWithMarker(R"(
        local ReplicatedStorage = game:GetService("ReplicatedStorage")

        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result);

    REQUIRE_EQ(imports.size(), 1);
    auto item = getItem(imports, "alphabet");
    REQUIRE(item);
    REQUIRE_EQ(item->additionalTextEdits.size(), 1);
    CHECK_EQ(item->additionalTextEdits[0].range.start.line, 2);
}

TEST_CASE_FIXTURE(Fixture, "auto_import_empty_require_statement")
{
    client->globalConfig.completion.imports.enabled = true;
    client->globalConfig.completion.imports.stringRequires.enabled = true;

    newDocument("alphabet.luau", "");

    auto [source, marker] = sourceWithMarker(R"(
        local Value = require()

        |
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto imports = filterAutoImports(result);

    REQUIRE_EQ(imports.size(), 1);
    auto item = getItem(imports, "alphabet");
    REQUIRE(item);
    REQUIRE_EQ(item->additionalTextEdits.size(), 1);
    CHECK_EQ(item->additionalTextEdits[0].range.start.line, 1);
}

TEST_SUITE_END();
