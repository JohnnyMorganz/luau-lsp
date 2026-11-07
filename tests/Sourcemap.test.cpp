#include "doctest.h"
#include "Fixture.h"
#include "RobloxTestConstants.h"
#include "Platform/RobloxPlatform.hpp"
#include "Protocol/Workspace.hpp"
#include "LuauFileUtils.hpp"

#include <random>

TEST_SUITE_BEGIN("SourcemapTests");

TEST_CASE("getScriptFilePath")
{
    SourceNode node("test", "ModuleScript", {"test.lua"}, {});
    CHECK_EQ(node.getScriptFilePath(), "test.lua");
}

TEST_CASE("getScriptFilePath returns json file if node is populated by JSON")
{
    SourceNode node("test", "ModuleScript", {"test.json"}, {});
    CHECK_EQ(node.getScriptFilePath(), "test.json");
}

TEST_CASE("getScriptFilePath returns toml file if node is populated by TOML")
{
    SourceNode node("test", "ModuleScript", {"test.toml"}, {});
    CHECK_EQ(node.getScriptFilePath(), "test.toml");
}

TEST_CASE("getScriptFilePath doesn't pick .meta.json")
{
    SourceNode node("init", "ModuleScript", {"init.meta.json", "init.lua"}, {});
    CHECK_EQ(node.getScriptFilePath(), "init.lua");
}

TEST_CASE_FIXTURE(Fixture, "can_access_children_via_dot_properties")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;
    loadSourcemap(R"(
            {
                "name": "Game",
                "className": "DataModel",
                "children": [
                    {
                        "name": "TemplateR15",
                        "className": "Part",
                        "children": [
                            {"name": "Head", "className": "Part"}
                        ]
                    }
                ]
            }
        )");

    auto result = check(R"(
        --!strict
        local template = game.TemplateR15
        local head = template.Head
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("template")) == "Part");
    CHECK(Luau::toString(requireType("head")) == "Part");
}

TEST_CASE_FIXTURE(Fixture, "can_access_children_via_find_first_child")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;
    loadSourcemap(R"(
            {
                "name": "Game",
                "className": "DataModel",
                "children": [
                    {
                        "name": "TemplateR15",
                        "className": "Part",
                        "children": [
                            {"name": "Head", "className": "Part"}
                        ]
                    }
                ]
            }
        )");

    auto result = check(R"(
        --!strict
        local template = game:FindFirstChild("TemplateR15")
        local head = template.Head
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("template")) == "Part");
    CHECK(Luau::toString(requireType("head")) == "Part");
}

TEST_CASE_FIXTURE(Fixture, "find_first_child_handles_unknown_child")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;
    loadSourcemap(R"(
            {
                "name": "Game",
                "className": "DataModel",
                "children": [
                    {
                        "name": "TemplateR15",
                        "className": "Part",
                        "children": [
                            {"name": "Head", "className": "Part"}
                        ]
                    }
                ]
            }
        )");

    auto result = check(R"(
        --!strict
        local template = game:FindFirstChild("Unknown")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("template")) == "Instance?");
}

TEST_CASE_FIXTURE(Fixture, "find_first_child_works_without_sourcemap")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;

    auto result = check(R"(
        --!strict
        local template = game:FindFirstChild("Unknown")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("template")) == "Instance?");
}


TEST_CASE_FIXTURE(Fixture, "find_first_child_supports_recursive_parameter_with_sourcemap")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;
    loadSourcemap(R"(
            {
                "name": "Game",
                "className": "DataModel",
                "children": [
                    {
                        "name": "TemplateR15",
                        "className": "Part",
                        "children": [
                            {"name": "Head", "className": "Part"}
                        ]
                    }
                ]
            }
        )");

    auto result = check(R"(
        --!strict
        local template = game:FindFirstChild("Head", true)
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("template")) == "Part");
}

TEST_CASE_FIXTURE(Fixture, "find_first_child_performs_bfs_and_picks_closest_matching_child_first")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;
    loadSourcemap(R"(
            {
                "name": "Game",
                "className": "DataModel",
                "children": [
                    {
                        "name": "Long",
                        "className": "Folder",
                        "children": [
                            {
                                "name": "Short",
                                "className": "Folder",
                                "children": [
                                    {"name": "Head", "className": "Folder"}
                                ]
                            }
                        ]
                    },
                    {
                        "name": "TemplateR15",
                        "className": "Part",
                        "children": [
                            {"name": "Head", "className": "Part"}
                        ]
                    }
                ]
            }
        )");

    auto result = check(R"(
        --!strict
        local template = game:FindFirstChild("Head", true)
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("template")) == "Part");
}

TEST_CASE_FIXTURE(Fixture, "find_first_child_still_supports_recursive_parameter_without_sourcemap")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;
    auto result = check(R"(
        --!strict
        local template = game:FindFirstChild("Unknown", true)
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("template")) == "Instance?");
}


TEST_CASE_FIXTURE(Fixture, "find_first_child_finds_direct_child_when_searching_recursively")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;
    loadSourcemap(R"(
            {
                "name": "Game",
                "className": "DataModel",
                "children": [
                    {
                        "name": "TemplateR15",
                        "className": "Part",
                        "children": [
                            {"name": "Head", "className": "Part"}
                        ]
                    }
                ]
            }
        )");

    auto result = check(R"(
        --!strict
        local template = game:FindFirstChild("TemplateR15", true)
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("template")) == "Part");
}

TEST_CASE_FIXTURE(Fixture, "can_access_children_via_wait_for_child")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;
    loadSourcemap(R"(
            {
                "name": "Game",
                "className": "DataModel",
                "children": [
                    {
                        "name": "TemplateR15",
                        "className": "Part",
                        "children": [
                            {"name": "Head", "className": "Part"}
                        ]
                    }
                ]
            }
        )");

    auto result = check(R"(
        --!strict
        local template = game:WaitForChild("TemplateR15")
        local head = template.Head
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("template")) == "Part");
    CHECK(Luau::toString(requireType("head")) == "Part");
}

TEST_CASE_FIXTURE(Fixture, "wait_for_child_handles_unknown_child")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;
    loadSourcemap(R"(
            {
                "name": "Game",
                "className": "DataModel",
                "children": [
                    {
                        "name": "TemplateR15",
                        "className": "Part",
                        "children": [
                            {"name": "Head", "className": "Part"}
                        ]
                    }
                ]
            }
        )");

    auto result = check(R"(
        --!strict
        local template = game:WaitForChild("UnknownChild")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("template")) == "Instance");
}

TEST_CASE_FIXTURE(Fixture, "wait_for_child_still_supports_timeout_parameter_with_sourcemap")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;
    loadSourcemap(R"(
            {
                "name": "Game",
                "className": "DataModel",
                "children": [
                    {
                        "name": "TemplateR15",
                        "className": "Part",
                        "children": [
                            {"name": "Head", "className": "Part"}
                        ]
                    }
                ]
            }
        )");

    auto result = check(R"(
        --!strict
        local template = game:WaitForChild("UnknownChild", 10)
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("template")) == "Instance?");
}

TEST_CASE_FIXTURE(Fixture, "wait_for_child_still_supports_timeout_parameter_without_sourcemap")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;

    auto result = check(R"(
        --!strict
        local template = game:WaitForChild("TemplateR15", 10)
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("template")) == "Instance?");
}

TEST_CASE_FIXTURE(Fixture, "wait_for_child_finds_direct_child_with_timeout_parameter")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;
    loadSourcemap(R"(
            {
                "name": "Game",
                "className": "DataModel",
                "children": [
                    {
                        "name": "TemplateR15",
                        "className": "Part",
                        "children": [
                            {"name": "Head", "className": "Part"}
                        ]
                    }
                ]
            }
        )");

    auto result = check(R"(
        --!strict
        local template = game:WaitForChild("TemplateR15", 10)
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("template")) == "Part");
}

TEST_CASE_FIXTURE(Fixture, "can_access_ancestor_via_find_first_ancestor")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;
    loadSourcemap(R"(
            {
                "name": "Game",
                "className": "DataModel",
                "children": [
                    {
                        "name": "TemplateR15",
                        "className": "Part",
                        "children": [
                            {
                                "name": "Head",
                                "className": "Part",
                                "children": [{ "name": "Attachment", "className": "Part" }]
                            }
                        ]
                    }
                ]
            }
        )");

    auto result = check(R"(
        --!strict
        local head = game.TemplateR15.Head.Attachment
        local template = head:FindFirstAncestor("TemplateR15")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("head")) == "Part");
    CHECK(Luau::toString(requireType("template")) == "Part");
}

TEST_CASE_FIXTURE(Fixture, "find_first_ancestor_handles_unknown_ancestor")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;
    loadSourcemap(R"(
            {
                "name": "Game",
                "className": "DataModel",
                "children": [
                    {
                        "name": "TemplateR15",
                        "className": "Part",
                        "children": [
                            {"name": "Head", "className": "Part"}
                        ]
                    }
                ]
            }
        )");

    auto result = check(R"(
        --!strict
        local head = game.TemplateR15.Head
        local random = head:FindFirstAncestor("Unknown")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("head")) == "Part");
    CHECK(Luau::toString(requireType("random")) == "Instance?");
}

TEST_CASE_FIXTURE(Fixture, "find_first_ancestor_works_without_sourcemap")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;

    auto result = check(R"(
        --!strict
        local template = game:FindFirstAncestor("Unknown")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("template")) == "Instance?");
}

TEST_CASE_FIXTURE(Fixture, "relative_and_absolute_types_are_consistent")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;
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
                                "name": "Shared",
                                "className": "Part",
                                "children": [{"name": "Part", "className": "Part"}, {"name": "Script", "className": "Instance"}]
                            }
                        ]
                    }
                ]
            }
        )");

    auto result = check(R"(
        --!strict
        local script: typeof(game.ReplicatedStorage.Shared.Script) -- mimic script
        local absolutePart = game:GetService("ReplicatedStorage"):FindFirstChild("Shared"):FindFirstChild("Part")
        local relativePart = script.Parent.Part
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);

    auto absoluteTy = requireType("absolutePart");
    auto relativeTy = requireType("relativePart");
    CHECK(Luau::toString(absoluteTy) == "Part");
    CHECK(Luau::toString(relativeTy) == "Part");
    CHECK((absoluteTy == relativeTy));
}

TEST_CASE_FIXTURE(Fixture, "get_virtual_module_name_from_real_path")
{
#ifdef _WIN32
    workspace.rootUri = Uri::parse("file:///c%3A/Users/Development/project");
    workspace.fileResolver.rootUri = Uri::parse("file:///c%3A/Users/Development/project");
    loadSourcemap(R"(
        {
            "name": "Game",
            "className": "DataModel",
            "children": [{"name": "MainScript", "className": "ModuleScript", "filePaths": ["Foo\\Test.luau"]}]
        }
    )");
#else
    workspace.rootUri = Uri::parse("file:///home/project");
    workspace.fileResolver.rootUri = Uri::parse("file:///home/project");
    loadSourcemap(R"(
        {
            "name": "Game",
            "className": "DataModel",
            "children": [{"name": "MainScript", "className": "ModuleScript", "filePaths": ["Foo/Test.luau"]}]
        }
    )");
#endif

    auto uri = workspace.rootUri.resolvePath("Foo/Test.luau");

    CHECK_EQ(workspace.fileResolver.getModuleName(uri), "game/MainScript");
}

TEST_CASE_FIXTURE(Fixture, "get_real_path_from_virtual_name")
{
#ifdef _WIN32
    workspace.rootUri = Uri::parse("file:///c%3A/Users/Development/project");
    workspace.fileResolver.rootUri = Uri::parse("file:///c%3A/Users/Development/project");
    loadSourcemap(R"(
        {
            "name": "Game",
            "className": "DataModel",
            "children": [{"name": "MainScript", "className": "ModuleScript", "filePaths": ["Foo\\Test.luau"]}]
        }
    )");
#else
    workspace.rootUri = Uri::parse("file:///random/project");
    workspace.fileResolver.rootUri = Uri::parse("file:///random/project");
    loadSourcemap(R"(
        {
            "name": "Game",
            "className": "DataModel",
            "children": [{"name": "MainScript", "className": "ModuleScript", "filePaths": ["Foo/Test.luau"]}]
        }
    )");
#endif

    CHECK_EQ(workspace.platform->resolveToRealPath("game/MainScript"), workspace.rootUri.resolvePath("Foo/Test.luau"));
}

TEST_CASE_FIXTURE(Fixture, "sourcemap_path_is_normalised_to_match_root_uri_subchild_with_lower_case_drive_letter")
{
#ifdef _WIN32
    workspace.rootUri = Uri::parse("file:///c%3A/Users/Development/project");
    workspace.fileResolver.rootUri = Uri::parse("file:///c%3A/Users/Development/project");
    loadSourcemap(R"(
        {
            "name": "RootNode",
            "className": "ModuleScript",
            "filePaths": ["Packages\\_Index\\example_package\\Test.luau"]
        }
    )");
#else
    workspace.rootUri = Uri::parse("file:///random/project");
    workspace.fileResolver.rootUri = Uri::parse("file:///random/project");
    loadSourcemap(R"(
        {
            "name": "RootNode",
            "className": "ModuleScript",
            "filePaths": ["Packages/_Index/example_package/Test.luau"]
        }
    )");
#endif

    auto rootNode = getRootSourceNode();
    auto filePath = rootNode->getScriptFilePath();
    REQUIRE(filePath);

    auto normalisedPath = dynamic_cast<RobloxPlatform*>(workspace.platform.get())->getRealPathFromSourceNode(rootNode);
    REQUIRE(normalisedPath);

    CHECK_EQ(workspace.rootUri.resolvePath(*filePath), normalisedPath);
}

TEST_CASE_FIXTURE(Fixture, "sourcemap_path_matches_ignore_globs")
{
#ifdef _WIN32
    workspace.rootUri = Uri::parse("file:///c%3A/Users/Development/project");
    workspace.fileResolver.rootUri = Uri::parse("file:///c%3A/Users/Development/project");
    loadSourcemap(R"(
        {
            "name": "RootNode",
            "className": "ModuleScript",
            "filePaths": ["Packages\\_Index\\example_package\\Test.luau"]
        }
    )");
#else
    workspace.rootUri = Uri::parse("file:///home/project");
    workspace.fileResolver.rootUri = Uri::parse("file:///home/project");
    loadSourcemap(R"(
        {
            "name": "RootNode",
            "className": "ModuleScript",
            "filePaths": ["Packages/_Index/example_package/Test.luau"]
        }
    )");
#endif
    client->globalConfig.completion.imports.ignoreGlobs = {"**/_Index/**"};


    auto rootNode = getRootSourceNode();
    auto filePath = dynamic_cast<RobloxPlatform*>(workspace.platform.get())->getRealPathFromSourceNode(rootNode);
    REQUIRE(filePath);

    CHECK(workspace.isIgnoredFileForAutoImports(*filePath));
}

TEST_CASE_FIXTURE(Fixture, "sourcemap_updates_marks_files_as_dirty")
{
    loadSourcemap(R"(
        {
            "name": "game",
            "className": "DataModel",
            "children": [
                {
                    "name": "Workspace",
                    "className": "Workspace",
                    "children": [{ "name": "Part", "className": "Part" }]
                }
            ]
        }
    )");

    auto document = newDocument("foo.luau", R"(
        local part = game.Workspace.Part
    )");

    lsp::HoverParams params;
    params.textDocument = {document};
    params.position = lsp::Position{1, 16};
    auto hover = workspace.hover(params, nullptr);

    REQUIRE(hover);
    CHECK_EQ(hover->contents.value, codeBlock("luau", "local part: Part"));

    loadSourcemap(R"(
        {
            "name": "game",
            "className": "DataModel",
            "children": [
                {
                    "name": "Workspace",
                    "className": "Workspace",
                    "children": [{ "name": "Part2", "className": "Part" }]
                }
            ]
        }
    )");

    auto hover2 = workspace.hover(params, nullptr);
    REQUIRE(hover2);
    if (FFlag::LuauSolverV2)
        CHECK_EQ(hover2->contents.value, codeBlock("luau", "local part: any"));
    else
        CHECK_EQ(hover2->contents.value, codeBlock("luau", "local part: *error-type*"));
}

TEST_CASE_FIXTURE(Fixture, "can_modify_the_parent_of_types_in_strict_mode")
{
    ENABLE_NEW_SOLVER();

    client->globalConfig.diagnostics.strictDatamodelTypes = true;
    loadSourcemap(R"(
        {
            "name": "game",
            "className": "DataModel",
            "children": [
                {
                    "name": "Workspace",
                    "className": "Workspace",
                    "children": [{ "name": "Part", "className": "Part" }]
                }
            ]
        }
    )");

    auto result = check(R"(
        --!strict
        local part = game.Workspace.Part
        part.Parent = Instance.new("TextLabel")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
}

TEST_CASE_FIXTURE(Fixture, "child_properties_of_services_are_cleared_when_the_service_is_removed_from_sourcemap")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;

    loadSourcemap(R"(
        {
            "name": "game",
            "className": "DataModel",
            "children": [
                {
                    "name": "ReplicatedStorage",
                    "className": "ReplicatedStorage",
                    "children": [{ "name": "Part", "className": "Part" }]
                }
            ]
        }
    )");

    auto source = R"(
        --!strict
        local ReplicatedStorage = game:GetService("ReplicatedStorage")
        print(ReplicatedStorage.Part)
    )";

    auto result = check(source);

    LUAU_LSP_REQUIRE_NO_ERRORS(result);

    loadSourcemap(R"(
        {
            "name": "game",
            "className": "DataModel",
            "children": []
        }
    )");

    auto result2 = check(source);

    REQUIRE_EQ(result2.errors.size(), 1);
    CHECK_EQ(Luau::get<Luau::UnknownProperty>(result2.errors[0])->key, "Part");
}

TEST_CASE_FIXTURE(Fixture, "child_properties_of_game_are_cleared_when_an_invalid_sourcemap_is_given")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;

    loadSourcemap(R"(
        {
            "name": "game",
            "className": "DataModel",
            "children": [
                {
                    "name": "Part",
                    "className": "Part"
                }
            ]
        }
    )");

    auto source = R"(
        --!strict
        print(game.Part)
    )";

    auto result = check(source);

    LUAU_LSP_REQUIRE_NO_ERRORS(result);

    loadSourcemap("");

    auto result2 = check(source);

    REQUIRE_EQ(result2.errors.size(), 1);
    CHECK_EQ(Luau::get<Luau::UnknownProperty>(result2.errors[0])->key, "Part");
}

TEST_CASE_FIXTURE(Fixture, "sourcemap_update_uses_plugin_info_if_sourcemap_file_is_missing")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;
    client->globalConfig.sourcemap.enabled = true;

    // Verify that no sourcemap file exists - this ensures we're testing the fallback behavior
    auto config = client->getConfiguration(workspace.rootUri);
    auto sourcemapPath = workspace.rootUri.resolvePath(config.sourcemap.sourcemapFile);
    REQUIRE_FALSE(Luau::FileUtils::exists(sourcemapPath.fsPath()));

    // Set up plugin info with a Part child
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    auto pluginData = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                {
                    "Name": "TestPart",
                    "ClassName": "Part"
                }
            ]
        }
    )");
    platform->onStudioPluginFullChange(pluginData);

    // Update sourcemap successfully
    REQUIRE(platform->updateSourceMap());

    // Verify the plugin info was used
    REQUIRE(platform->rootSourceNode);
    CHECK_EQ(platform->rootSourceNode->className, "DataModel");

    auto result = check(R"(
        --!strict
        local part = game.TestPart
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("part")) == "Part");
}

TEST_CASE_FIXTURE(Fixture, "plugin_info_hydrates_existing_sourcemap_and_marks_nodes_plugin_managed")
{
    // First load a filesystem sourcemap with an existing child
    loadSourcemap(R"(
        {
            "name": "game",
            "className": "DataModel",
            "children": [
                {
                    "name": "ReplicatedStorage",
                    "className": "ReplicatedStorage",
                    "filePaths": ["src/ReplicatedStorage.luau"]
                }
            ]
        }
    )");

    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(platform->rootSourceNode);

    // Verify initial state - ReplicatedStorage exists and is NOT plugin managed
    auto rsNode = platform->rootSourceNode->findChild("ReplicatedStorage");
    REQUIRE(rsNode);
    CHECK_FALSE((*rsNode)->pluginManaged);

    // Now apply plugin info that adds a new child
    platform->pluginNodeAllocator.clear();
    auto pluginData = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                {
                    "Name": "ServerStorage",
                    "ClassName": "ServerStorage"
                }
            ]
        }
    )");
    platform->onStudioPluginFullChange(pluginData);

    // Verify that ReplicatedStorage still exists and is still NOT plugin managed
    rsNode = platform->rootSourceNode->findChild("ReplicatedStorage");
    REQUIRE(rsNode);
    CHECK_FALSE((*rsNode)->pluginManaged);

    // Verify that ServerStorage was added and IS plugin managed
    auto ssNode = platform->rootSourceNode->findChild("ServerStorage");
    REQUIRE(ssNode);
    CHECK((*ssNode)->pluginManaged);
    CHECK_EQ((*ssNode)->className, "ServerStorage");
}

TEST_CASE_FIXTURE(Fixture, "plugin_info_creates_datamodel_root_when_no_sourcemap_exists")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());

    // Ensure no sourcemap exists initially
    REQUIRE_FALSE(platform->rootSourceNode);

    // Apply plugin info without any filesystem sourcemap
    auto pluginData = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                {
                    "Name": "Workspace",
                    "ClassName": "Workspace",
                    "Children": [
                        {
                            "Name": "Part",
                            "ClassName": "Part"
                        }
                    ]
                }
            ]
        }
    )");
    platform->onStudioPluginFullChange(pluginData);

    REQUIRE(platform->rootSourceNode);
    CHECK_EQ(platform->rootSourceNode->name, "game");
    CHECK_EQ(platform->rootSourceNode->className, "DataModel");

    // Verify children were created
    auto workspaceNode = platform->rootSourceNode->findChild("Workspace");
    REQUIRE(workspaceNode);
    CHECK((*workspaceNode)->pluginManaged);

    auto partNode = (*workspaceNode)->findChild("Part");
    REQUIRE(partNode);
    CHECK((*partNode)->pluginManaged);
}

TEST_CASE_FIXTURE(Fixture, "plugin_clear_removes_plugin_managed_nodes_only")
{
    // Load a sourcemap with a filesystem-sourced child
    loadSourcemap(R"(
        {
            "name": "game",
            "className": "DataModel",
            "children": [
                {
                    "name": "ReplicatedStorage",
                    "className": "ReplicatedStorage",
                    "filePaths": ["src/ReplicatedStorage.luau"]
                }
            ]
        }
    )");

    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(platform->rootSourceNode);

    // Apply plugin info that adds a plugin-managed child
    auto pluginData = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                {
                    "Name": "ServerStorage",
                    "ClassName": "ServerStorage"
                }
            ]
        }
    )");
    platform->onStudioPluginFullChange(pluginData);

    // Verify both children exist
    REQUIRE(platform->rootSourceNode->findChild("ReplicatedStorage"));
    REQUIRE(platform->rootSourceNode->findChild("ServerStorage"));

    // Clear plugin-managed nodes (simulates plugin disconnect)
    platform->onStudioPluginClear();

    // Verify filesystem-sourced child still exists
    auto rsNode = platform->rootSourceNode->findChild("ReplicatedStorage");
    REQUIRE(rsNode);
    CHECK_FALSE((*rsNode)->pluginManaged);

    // Verify plugin-managed child was removed
    CHECK_FALSE(platform->rootSourceNode->findChild("ServerStorage"));
}

TEST_CASE_FIXTURE(Fixture, "plugin_managed_flag_persists_through_sourcemap_reload")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;

    // Load a sourcemap that includes pluginManaged flags (simulating a previously saved sourcemap)
    loadSourcemap(R"(
        {
            "name": "game",
            "className": "DataModel",
            "children": [
                {
                    "name": "ReplicatedStorage",
                    "className": "ReplicatedStorage",
                    "filePaths": ["src/ReplicatedStorage.luau"]
                },
                {
                    "name": "ServerStorage",
                    "className": "ServerStorage",
                    "pluginManaged": true
                }
            ]
        }
    )");

    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(platform->rootSourceNode);

    // Verify ReplicatedStorage is NOT plugin managed
    auto rsNode = platform->rootSourceNode->findChild("ReplicatedStorage");
    REQUIRE(rsNode);
    CHECK_FALSE((*rsNode)->pluginManaged);

    // Verify ServerStorage IS plugin managed (flag persisted from JSON)
    auto ssNode = platform->rootSourceNode->findChild("ServerStorage");
    REQUIRE(ssNode);
    CHECK((*ssNode)->pluginManaged);
}

TEST_CASE_FIXTURE(Fixture, "sourcemap_autogenerate_writes_file_when_plugin_info_applied")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;
    client->globalConfig.sourcemap.enabled = true;
    client->globalConfig.sourcemap.autogenerate = true;

    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());

    // Create a sourcemap file first to establish the root node
    auto sourcemapPath = tempDir.write_child("sourcemap.json", R"({
        "name": "game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "filePaths": ["src/shared/init.luau"]
            }
        ]
    })");

    // Load the sourcemap from file
    platform->updateSourceMap();

    // Apply plugin info with filePaths
    auto pluginData = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                {
                    "Name": "ServerStorage",
                    "ClassName": "ServerStorage",
                    "FilePaths": ["src/server/init.luau"]
                }
            ]
        }
    )");
    platform->onStudioPluginFullChange(pluginData);

    // Verify the sourcemap file was updated
    auto updatedContents = Luau::FileUtils::readFile(sourcemapPath);
    REQUIRE(updatedContents);

    auto updatedJson = json::parse(*updatedContents);
    CHECK_EQ(updatedJson["name"], "game");
    CHECK_EQ(updatedJson["className"], "DataModel");

    // Check that both children are in the file
    REQUIRE(updatedJson.contains("children"));
    auto& children = updatedJson["children"];

    bool hasReplicatedStorage = false;
    bool hasServerStorage = false;

    for (const auto& child : children)
    {
        if (child["name"] == "ReplicatedStorage")
        {
            hasReplicatedStorage = true;
            CHECK_EQ(child["className"], "ReplicatedStorage");
        }
        if (child["name"] == "ServerStorage")
        {
            hasServerStorage = true;
            CHECK_EQ(child["className"], "ServerStorage");
            CHECK(child.contains("pluginManaged"));
            CHECK_EQ(child["pluginManaged"], true);
        }
    }

    CHECK(hasReplicatedStorage);
    CHECK(hasServerStorage);
}

TEST_CASE_FIXTURE(Fixture, "plugin_info_updates_file_paths_on_existing_nodes")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;

    // Load sourcemap with a node that has filePaths
    loadSourcemap(R"(
        {
            "name": "game",
            "className": "DataModel",
            "children": [
                {
                    "name": "ReplicatedStorage",
                    "className": "ReplicatedStorage",
                    "filePaths": ["src/shared/init.luau"]
                }
            ]
        }
    )");

    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(platform->rootSourceNode);

    // Verify initial filePaths
    auto rsNode = platform->rootSourceNode->findChild("ReplicatedStorage");
    REQUIRE(rsNode);
    CHECK_EQ((*rsNode)->filePaths.size(), 1);

    // Apply plugin info that updates the filePaths for the same node
    // The condition is: (pluginManaged || !pluginFilePaths.empty()) && filePaths != pluginFilePaths
    // Since plugin has non-empty filePaths, it WILL update them
    auto pluginData = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                {
                    "Name": "ReplicatedStorage",
                    "ClassName": "ReplicatedStorage",
                    "FilePaths": ["src/shared/updated.luau", "src/shared/extra.luau"]
                }
            ]
        }
    )");
    platform->onStudioPluginFullChange(pluginData);

    // Verify filePaths were updated
    rsNode = platform->rootSourceNode->findChild("ReplicatedStorage");
    REQUIRE(rsNode);
    CHECK_EQ((*rsNode)->filePaths.size(), 2);
}

TEST_CASE_FIXTURE(Fixture, "source_node_to_json_only_includes_nodes_with_file_paths")
{
    // Create a source node tree with some nodes having filePaths and some without
    Luau::TypedAllocator<SourceNode> allocator;

    auto child1 = allocator.allocate(SourceNode("ModuleA", "ModuleScript", {"src/ModuleA.luau"}, {}));
    auto child2 = allocator.allocate(SourceNode("PartNoFile", "Part", {}, {}));
    auto child3 = allocator.allocate(SourceNode("ModuleB", "ModuleScript", {"src/ModuleB.luau"}, {}));

    auto root = allocator.allocate(SourceNode("game", "DataModel", {}, {child1, child2, child3}));

    auto jsonOutput = root->toJson();

    CHECK_EQ(jsonOutput["name"], "game");
    CHECK_EQ(jsonOutput["className"], "DataModel");
    REQUIRE(jsonOutput.contains("children"));

    // Only nodes with filePaths should be in the output
    auto& children = jsonOutput["children"];
    CHECK_EQ(children.size(), 2);

    bool hasModuleA = false;
    bool hasModuleB = false;
    bool hasPartNoFile = false;

    for (const auto& child : children)
    {
        if (child["name"] == "ModuleA")
            hasModuleA = true;
        if (child["name"] == "ModuleB")
            hasModuleB = true;
        if (child["name"] == "PartNoFile")
            hasPartNoFile = true;
    }

    CHECK(hasModuleA);
    CHECK(hasModuleB);
    CHECK_FALSE(hasPartNoFile);
}

TEST_CASE_FIXTURE(Fixture, "source_node_to_json_includes_plugin_managed_flag")
{
    Luau::TypedAllocator<SourceNode> allocator;

    auto child = allocator.allocate(SourceNode("ServerStorage", "ServerStorage", {"src/server.luau"}, {}));
    child->pluginManaged = true;

    auto root = allocator.allocate(SourceNode("game", "DataModel", {}, {child}));

    auto jsonOutput = root->toJson();

    REQUIRE(jsonOutput.contains("children"));
    auto& children = jsonOutput["children"];
    REQUIRE_EQ(children.size(), 1);

    CHECK(children[0].contains("pluginManaged"));
    CHECK_EQ(children[0]["pluginManaged"], true);
}

TEST_CASE_FIXTURE(Fixture, "on_studio_plugin_full_change_updates_sourcemap")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());

    // Simulate a full plugin change notification
    auto pluginData = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                {
                    "Name": "Workspace",
                    "ClassName": "Workspace",
                    "Children": [
                        {
                            "Name": "SpawnLocation",
                            "ClassName": "SpawnLocation"
                        }
                    ]
                }
            ]
        }
    )");

    platform->onStudioPluginFullChange(pluginData);

    REQUIRE(platform->rootSourceNode);
    CHECK_EQ(platform->rootSourceNode->className, "DataModel");

    auto workspaceNode = platform->rootSourceNode->findChild("Workspace");
    REQUIRE(workspaceNode);
    CHECK((*workspaceNode)->pluginManaged);

    auto spawnNode = (*workspaceNode)->findChild("SpawnLocation");
    REQUIRE(spawnNode);
    CHECK((*spawnNode)->pluginManaged);
}

TEST_CASE_FIXTURE(Fixture, "handle_notification_routes_plugin_full_notification")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());

    auto pluginData = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                {
                    "Name": "TestService",
                    "ClassName": "TestService"
                }
            ]
        }
    )");

    // Call handleNotification with $/plugin/full method
    bool handled = platform->handleNotification("$/plugin/full", pluginData);

    CHECK(handled);
    REQUIRE(platform->rootSourceNode);
    CHECK_EQ(platform->rootSourceNode->className, "DataModel");

    auto testServiceNode = platform->rootSourceNode->findChild("TestService");
    REQUIRE(testServiceNode);
    CHECK((*testServiceNode)->pluginManaged);
}

TEST_CASE_FIXTURE(Fixture, "handle_notification_routes_plugin_clear_notification")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());

    // First set up a sourcemap with a plugin-managed child
    loadSourcemap(R"(
        {
            "name": "game",
            "className": "DataModel",
            "children": [
                {
                    "name": "ReplicatedStorage",
                    "className": "ReplicatedStorage",
                    "filePaths": ["src/shared.luau"]
                }
            ]
        }
    )");

    // Add a plugin-managed child
    auto pluginData = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                {
                    "Name": "ServerStorage",
                    "ClassName": "ServerStorage"
                }
            ]
        }
    )");
    platform->onStudioPluginFullChange(pluginData);

    REQUIRE(platform->rootSourceNode->findChild("ServerStorage"));

    // Call handleNotification with $/plugin/clear method
    bool handled = platform->handleNotification("$/plugin/clear", std::nullopt);

    CHECK(handled);
    // Plugin-managed child should be removed
    CHECK_FALSE(platform->rootSourceNode->findChild("ServerStorage"));
    // Filesystem-sourced child should remain
    REQUIRE(platform->rootSourceNode->findChild("ReplicatedStorage"));
}


TEST_CASE_FIXTURE(Fixture, "plugin_prunes_children_removed_from_plugin_info")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());

    // First plugin update adds two children
    auto pluginData1 = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                { "Name": "ChildA", "ClassName": "Folder" },
                { "Name": "ChildB", "ClassName": "Folder" }
            ]
        }
    )");
    platform->onStudioPluginFullChange(pluginData1);

    REQUIRE(platform->rootSourceNode);
    REQUIRE(platform->rootSourceNode->findChild("ChildA"));
    REQUIRE(platform->rootSourceNode->findChild("ChildB"));

    // Second plugin update removes ChildB
    auto pluginData2 = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                { "Name": "ChildA", "ClassName": "Folder" }
            ]
        }
    )");
    platform->onStudioPluginFullChange(pluginData2);

    // ChildA should still exist
    REQUIRE(platform->rootSourceNode->findChild("ChildA"));
    // ChildB should be pruned
    CHECK_FALSE(platform->rootSourceNode->findChild("ChildB"));
}

TEST_CASE_FIXTURE(Fixture, "plugin_update_removes_stale_path_map_entries_for_pruned_nodes")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());

    auto pluginData1 = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                { "Name": "ChildA", "ClassName": "Folder" },
                { "Name": "ChildB", "ClassName": "ModuleScript", "FilePaths": ["ChildB.luau"] }
            ]
        }
    )");
    platform->onStudioPluginFullChange(pluginData1);

    auto childBUri = workspace.rootUri.resolvePath("ChildB.luau");
    REQUIRE(platform->virtualPathsToSourceNodes.count("game/ChildB") == 1);
    REQUIRE(platform->resolveToVirtualPath(childBUri) == "game/ChildB");

    // Second plugin update prunes ChildB
    auto pluginData2 = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                { "Name": "ChildA", "ClassName": "Folder" }
            ]
        }
    )");
    platform->onStudioPluginFullChange(pluginData2);

    // The path maps must not retain entries pointing at the pruned node, otherwise later
    // type checks can resolve the node and resurrect its freed cached types
    CHECK(platform->virtualPathsToSourceNodes.count("game/ChildB") == 0);
    CHECK_FALSE(platform->resolveToVirtualPath(childBUri));
}

TEST_CASE_FIXTURE(Fixture, "plugin_update_clears_cached_types_on_pruned_nodes")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());

    auto pluginData1 = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                { "Name": "ChildB", "ClassName": "Folder" }
            ]
        }
    )");
    platform->onStudioPluginFullChange(pluginData1);

    auto childB = platform->rootSourceNode->findChild("ChildB");
    REQUIRE(childB);
    const SourceNode* childBNode = *childB;

    // Simulate a type having been generated and cached for this node during a previous check
    childBNode->tys.insert_or_assign(&workspace.frontend.globals, workspace.frontend.builtinTypes->anyType);

    // Second plugin update prunes ChildB. The node stays allocated but is detached from the
    // tree, so clearSourcemapTypes can no longer reach it - the cache must be cleared on prune
    auto pluginData2 = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": []
        }
    )");
    platform->onStudioPluginFullChange(pluginData2);

    CHECK_FALSE(platform->rootSourceNode->findChild("ChildB"));
    CHECK(childBNode->tys.empty());
}

TEST_CASE_FIXTURE(Fixture, "plugin_clear_clears_cached_types_on_pruned_nodes")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());

    auto pluginData = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                { "Name": "ChildB", "ClassName": "ModuleScript", "FilePaths": ["ChildB.luau"] }
            ]
        }
    )");
    platform->onStudioPluginFullChange(pluginData);

    auto childB = platform->rootSourceNode->findChild("ChildB");
    REQUIRE(childB);
    const SourceNode* childBNode = *childB;
    childBNode->tys.insert_or_assign(&workspace.frontend.globals, workspace.frontend.builtinTypes->anyType);

    auto childBUri = workspace.rootUri.resolvePath("ChildB.luau");
    REQUIRE(platform->resolveToVirtualPath(childBUri) == "game/ChildB");

    platform->onStudioPluginClear();

    CHECK_FALSE(platform->rootSourceNode->findChild("ChildB"));
    CHECK(childBNode->tys.empty());
    CHECK(platform->virtualPathsToSourceNodes.count("game/ChildB") == 0);
    CHECK_FALSE(platform->resolveToVirtualPath(childBUri));
}

// Use-after-free regression test: without proper cleanup on prune, the pruned node remains
// reachable through stale path map entries and its cached types point into the destroyed
// instanceTypes arena, so a later check binds `script` to a freed type (crashes under ASAN)
TEST_CASE_FIXTURE(Fixture, "plugin_prune_does_not_leave_dangling_types_reachable_by_later_checks")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;

    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());

    auto pluginData1 = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                {
                    "Name": "Folder",
                    "ClassName": "Folder",
                    "Children": [
                        { "Name": "Script", "ClassName": "ModuleScript", "FilePaths": ["script.luau"] }
                    ]
                }
            ]
        }
    )");
    platform->onStudioPluginFullChange(pluginData1);

    // Check a document backed by the plugin-provided node: `script` is bound to a
    // sourcemap-generated type, which gets cached on the source node
    auto uri = newDocument("script.luau", R"(
        local p = script.Parent
        return {}
    )");
    workspace.frontend.check(workspace.fileResolver.getModuleName(uri));

    // Prune the subtree. This destroys and rebuilds the instanceTypes arena, and internally
    // triggers a recheck of the (dirty) managed document via recomputeDiagnostics
    auto pluginData2 = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": []
        }
    )");
    platform->onStudioPluginFullChange(pluginData2);

    // A later fresh check must also not be able to reach the pruned node
    auto moduleName = workspace.fileResolver.getModuleName(uri);
    workspace.frontend.markDirty(moduleName);
    workspace.frontend.check(moduleName);
    CHECK(workspace.frontend.getSourceModule(moduleName));
}

TEST_CASE_FIXTURE(Fixture, "sourcemap_update_invalidates_stale_module_type_graphs")
{
    // Use an unmanaged on-disk file: recomputeDiagnostics (triggered by the sourcemap update
    // when expressive DataModel types are enabled) would immediately recheck a managed file,
    // which prevents observing the invalidation
    auto uri = Uri::file(tempDir.write_child("foo.luau", "local x = 1"));
    auto moduleName = workspace.fileResolver.getModuleName(uri);

    workspace.frontend.check(moduleName);
    REQUIRE(workspace.frontend.allModuleDependenciesValid(moduleName, /* forAutocomplete= */ false));

    loadSourcemap(R"(
        {
            "name": "game",
            "className": "DataModel",
            "children": []
        }
    )");

    // The retained type graph of an already-checked module may reference the sourcemap types
    // that were just destroyed. It must not be considered valid for incremental (fragment)
    // autocomplete until the module has been rechecked
    CHECK_FALSE(workspace.frontend.allModuleDependenciesValid(moduleName, /* forAutocomplete= */ false));
    CHECK_FALSE(workspace.frontend.allModuleDependenciesValid(moduleName, /* forAutocomplete= */ true));

    // A recheck makes the module valid again
    workspace.frontend.check(moduleName);
    CHECK(workspace.frontend.allModuleDependenciesValid(moduleName, /* forAutocomplete= */ false));
}

TEST_CASE_FIXTURE(Fixture, "nested_plugin_children_are_all_marked_plugin_managed")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());

    auto pluginData = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                {
                    "Name": "Level1",
                    "ClassName": "Folder",
                    "Children": [
                        {
                            "Name": "Level2",
                            "ClassName": "Folder",
                            "Children": [
                                {
                                    "Name": "Level3",
                                    "ClassName": "Part"
                                }
                            ]
                        }
                    ]
                }
            ]
        }
    )");
    platform->onStudioPluginFullChange(pluginData);

    REQUIRE(platform->rootSourceNode);

    auto level1 = platform->rootSourceNode->findChild("Level1");
    REQUIRE(level1);
    CHECK((*level1)->pluginManaged);

    auto level2 = (*level1)->findChild("Level2");
    REQUIRE(level2);
    CHECK((*level2)->pluginManaged);

    auto level3 = (*level2)->findChild("Level3");
    REQUIRE(level3);
    CHECK((*level3)->pluginManaged);
}

TEST_CASE("source_node_contains_file_paths_returns_true_when_node_has_file_paths")
{
    Luau::TypedAllocator<SourceNode> allocator;

    auto node = allocator.allocate(SourceNode("Module", "ModuleScript", {"src/module.luau"}, {}));

    CHECK(node->containsFilePaths());
}

TEST_CASE("source_node_contains_file_paths_returns_true_when_descendant_has_file_paths")
{
    Luau::TypedAllocator<SourceNode> allocator;

    auto child = allocator.allocate(SourceNode("Module", "ModuleScript", {"src/module.luau"}, {}));
    auto parent = allocator.allocate(SourceNode("Folder", "Folder", {}, {child}));
    auto root = allocator.allocate(SourceNode("game", "DataModel", {}, {parent}));

    CHECK(root->containsFilePaths());
    CHECK(parent->containsFilePaths());
    CHECK(child->containsFilePaths());
}

TEST_CASE("source_node_contains_file_paths_returns_false_when_no_file_paths_in_tree")
{
    Luau::TypedAllocator<SourceNode> allocator;

    auto child = allocator.allocate(SourceNode("Part", "Part", {}, {}));
    auto parent = allocator.allocate(SourceNode("Folder", "Folder", {}, {child}));
    auto root = allocator.allocate(SourceNode("game", "DataModel", {}, {parent}));

    CHECK_FALSE(root->containsFilePaths());
    CHECK_FALSE(parent->containsFilePaths());
    CHECK_FALSE(child->containsFilePaths());
}

TEST_CASE_FIXTURE(Fixture, "plugin_node_from_json_parses_file_paths")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());

    auto pluginData = json::parse(R"(
        {
            "Name": "Module",
            "ClassName": "ModuleScript",
            "FilePaths": ["src/module.luau", "src/module.meta.json"]
        }
    )");

    platform->pluginNodeAllocator.clear();
    auto pluginNode = PluginNode::fromJson(pluginData, platform->pluginNodeAllocator);

    CHECK_EQ(pluginNode->name, "Module");
    CHECK_EQ(pluginNode->className, "ModuleScript");
    CHECK_EQ(pluginNode->filePaths.size(), 2);
}

TEST_CASE_FIXTURE(Fixture, "plugin_node_from_json_handles_empty_file_paths")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());

    auto pluginData = json::parse(R"(
        {
            "Name": "Folder",
            "ClassName": "Folder",
            "FilePaths": []
        }
    )");

    platform->pluginNodeAllocator.clear();
    auto pluginNode = PluginNode::fromJson(pluginData, platform->pluginNodeAllocator);

    CHECK_EQ(pluginNode->filePaths.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "plugin_node_from_json_handles_missing_file_paths")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());

    auto pluginData = json::parse(R"(
        {
            "Name": "Part",
            "ClassName": "Part"
        }
    )");

    platform->pluginNodeAllocator.clear();
    auto pluginNode = PluginNode::fromJson(pluginData, platform->pluginNodeAllocator);

    CHECK_EQ(pluginNode->filePaths.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "plugin_file_paths_propagate_to_source_node_during_hydration")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());

    // Create a sourcemap with an empty filePaths node
    loadSourcemap(R"(
        {
            "name": "game",
            "className": "DataModel",
            "children": []
        }
    )");

    // Apply plugin info with filePaths
    auto pluginData = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                {
                    "Name": "Module",
                    "ClassName": "ModuleScript",
                    "FilePaths": ["src/module.luau"]
                }
            ]
        }
    )");
    platform->onStudioPluginFullChange(pluginData);

    auto moduleNode = platform->rootSourceNode->findChild("Module");
    REQUIRE(moduleNode);
    CHECK_EQ((*moduleNode)->filePaths.size(), 1);
}

TEST_CASE_FIXTURE(Fixture, "sourcemap_file_change_detection_works_with_simple_filename")
{
    client->globalConfig.sourcemap.sourcemapFile = "sourcemap.json";
    client->notificationQueue.clear();

    lsp::FileEvent event;
    event.uri = workspace.rootUri.resolvePath("sourcemap.json");
    event.type = lsp::FileChangeType::Changed;

    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    platform->onDidChangeWatchedFiles(event);

    bool foundLogMessage = false;
    for (const auto& [method, params] : client->notificationQueue)
    {
        if (method == "window/logMessage" && params)
        {
            auto message = params->value("message", "");
            if (message.find("Registering sourcemap changed") != std::string::npos)
            {
                foundLogMessage = true;
                break;
            }
        }
    }
    CHECK(foundLogMessage);
}

TEST_CASE_FIXTURE(Fixture, "sourcemap_file_change_detection_works_with_relative_paths")
{
    client->globalConfig.sourcemap.sourcemapFile = "subdir/sourcemap.json";
    client->notificationQueue.clear();

    lsp::FileEvent event;
    event.uri = workspace.rootUri.resolvePath("subdir/sourcemap.json");
    event.type = lsp::FileChangeType::Changed;

    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    platform->onDidChangeWatchedFiles(event);

    bool foundLogMessage = false;
    for (const auto& [method, params] : client->notificationQueue)
    {
        if (method == "window/logMessage" && params)
        {
            auto message = params->value("message", "");
            if (message.find("Registering sourcemap changed") != std::string::npos)
            {
                foundLogMessage = true;
                break;
            }
        }
    }
    CHECK(foundLogMessage);
}

TEST_CASE_FIXTURE(Fixture, "plugin_update_clears_cached_sourcemap_types_on_nodes")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    loadSourcemap(R"(
        {
            "name": "game",
            "className": "DataModel",
            "children": [
                {
                    "name": "ReplicatedStorage",
                    "className": "ReplicatedStorage",
                    "children": [
                        {
                            "name": "Shared",
                            "className": "Folder",
                            "children": [{ "name": "Remotes", "className": "Part" }]
                        }
                    ]
                }
            ]
        }
    )");
    auto originalRootNode = platform->rootSourceNode;

    auto document = newDocument("foo.luau", R"(
        local ReplicatedStorage = game:GetService("ReplicatedStorage")
        local Remotes = require(ReplicatedStorage.Shared.Remotes)
    )");

    lsp::HoverParams params;
    params.textDocument = {document};
    params.position = lsp::Position{2, 59};
    auto hover = workspace.hover(params, nullptr);

    REQUIRE(hover);
    CHECK_EQ(hover->contents.value, codeBlock("luau", "Part"));

    auto pluginData = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                {
                    "Name": "ReplicatedStorage",
                    "ClassName": "ReplicatedStorage",
                    "FilePaths": [],
                    "Children": [
                        {
                            "Name": "SharedModule",
                            "ClassName": "Part",
                            "FilePaths": ["src/ReplicatedStorage/SharedModule.luau"],
                            "Children": []
                        }
                    ]
                }
            ]
        }
    )");
    platform->onStudioPluginFullChange(pluginData);

    // After a plugin update, we still re-use the old root source node
    CHECK_EQ(originalRootNode, platform->rootSourceNode);

    auto hover2 = workspace.hover(params, nullptr);
    REQUIRE(hover2);
    CHECK_EQ(hover2->contents.value, codeBlock("luau", "Part"));
}

TEST_CASE_FIXTURE(Fixture, "source_node_get_script_context_resolution")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(platform);

    auto instanceData = json::parse(R"(
        {
            "Name": "game",
            "ClassName": "DataModel",
            "Children": [
                {
                    "Name": "Workspace",
                    "ClassName": "Workspace",
                    "Children": [
                        { "Name": "SharedModule", "ClassName": "ModuleScript" },
                        { "Name": "LocalScriptInWorkspace", "ClassName": "LocalScript" },
                        { "Name": "ScriptInWorkspace", "ClassName": "Script" }
                    ]
                },
                {
                    "Name": "ServerScriptService",
                    "ClassName": "ServerScriptService",
                    "Children": [
                        {
                            "Name": "Folder",
                            "ClassName": "Folder",
                            "Children": [
                                { "Name": "NestedServerModule", "ClassName": "ModuleScript" }
                            ]
                        }
                    ]
                },
                {
                    "Name": "StarterPlayer",
                    "ClassName": "StarterPlayer",
                    "Children": [
                        { "Name": "ClientModule", "ClassName": "ModuleScript" }
                    ]
                }
            ]
        }
    )");

    platform->onStudioPluginFullChange(instanceData);
    REQUIRE(platform->rootSourceNode);
    auto root = platform->rootSourceNode;

    auto workspaceNode = root->findChild("Workspace");
    REQUIRE(workspaceNode);
    auto sharedModule = (*workspaceNode)->findChild("SharedModule");
    REQUIRE(sharedModule);
    CHECK_EQ((*sharedModule)->scriptContext, ScriptContext::Shared);

    auto localInWorkspace = (*workspaceNode)->findChild("LocalScriptInWorkspace");
    REQUIRE(localInWorkspace);
    CHECK_EQ((*localInWorkspace)->scriptContext, ScriptContext::Client);

    auto scriptInWorkspace = (*workspaceNode)->findChild("ScriptInWorkspace");
    REQUIRE(scriptInWorkspace);
    CHECK_EQ((*scriptInWorkspace)->scriptContext, ScriptContext::Server);

    auto sss = root->findChild("ServerScriptService");
    REQUIRE(sss);
    auto folder = (*sss)->findChild("Folder");
    REQUIRE(folder);
    auto nestedServerModule = (*folder)->findChild("NestedServerModule");
    REQUIRE(nestedServerModule);
    CHECK_EQ((*nestedServerModule)->scriptContext, ScriptContext::Server);

    auto starterPlayer = root->findChild("StarterPlayer");
    REQUIRE(starterPlayer);
    auto clientModule = (*starterPlayer)->findChild("ClientModule");
    REQUIRE(clientModule);
    CHECK_EQ((*clientModule)->scriptContext, ScriptContext::Client);
}


TEST_CASE_FIXTURE(Fixture, "script_context_resolution_respects_file_path_suffixes")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(platform);

    loadSourcemap(SOURCEMAP_FOR_RUN_CONTEXT_AUTO_IMPORTS);
    REQUIRE(platform->rootSourceNode);
    auto root = platform->rootSourceNode;

    // RunContext-style client script: Script class + .client.luau suffix -> Client
    auto replicatedStorage = root->findChild("ReplicatedStorage");
    REQUIRE(replicatedStorage);
    auto features = (*replicatedStorage)->findChild("Features");
    REQUIRE(features);
    auto mainScript = (*features)->findChild("main");
    REQUIRE(mainScript);
    CHECK_EQ((*mainScript)->scriptContext, ScriptContext::Client);

    // RunContext-style server script: Script class + .server.luau suffix -> Server
    auto bootScript = (*features)->findChild("boot");
    REQUIRE(bootScript);
    CHECK_EQ((*bootScript)->scriptContext, ScriptContext::Server);

    // A folder synced from init.client.luau: the suffix is on the init file
    auto widget = (*features)->findChild("Widget");
    REQUIRE(widget);
    CHECK_EQ((*widget)->scriptContext, ScriptContext::Client);

    // No suffix: class name fallback is preserved
    auto workspaceNode = root->findChild("Workspace");
    REQUIRE(workspaceNode);
    auto legacyScript = (*workspaceNode)->findChild("LegacyScript");
    REQUIRE(legacyScript);
    CHECK_EQ((*legacyScript)->scriptContext, ScriptContext::Server);

    // LocalScript: unconditionally Client (class invariant, suffix never consulted)
    auto localScript = (*workspaceNode)->findChild("MyLocalScript");
    REQUIRE(localScript);
    CHECK_EQ((*localScript)->scriptContext, ScriptContext::Client);

    // ModuleScript still inherits from its container
    auto sharedModule = (*replicatedStorage)->findChild("SharedModule");
    REQUIRE(sharedModule);
    CHECK_EQ((*sharedModule)->scriptContext, ScriptContext::Shared);
}

static json pluginChanges(const std::string& session, const json& changes)
{
    return json{{"session", session}, {"changes", changes}};
}

static const json kPluginDataModelWithIds = json::parse(R"(
    {
        "Id": 1,
        "Session": "session-1",
        "Name": "game",
        "ClassName": "DataModel",
        "Children": [
            {
                "Id": 2,
                "Name": "ReplicatedStorage",
                "ClassName": "ReplicatedStorage",
                "Children": [
                    { "Id": 3, "Name": "Assets", "ClassName": "Folder", "Children": [{ "Id": 4, "Name": "Sword", "ClassName": "Tool" }] }
                ]
            },
            { "Id": 5, "Name": "Workspace", "ClassName": "Workspace" }
        ]
    }
)");

TEST_CASE_FIXTURE(Fixture, "plugin_changes_add_rename_reparent_and_remove_instances")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    platform->onStudioPluginFullChange(kPluginDataModelWithIds);

    auto findPath = [&](const std::string& path) -> const SourceNode*
    {
        return platform->rootSourceNode->walkPath(path);
    };
    REQUIRE(findPath("ReplicatedStorage/Assets/Sword"));

    // Add a subtree
    REQUIRE(platform->onStudioPluginChanges(pluginChanges("session-1", json::parse(R"([
        {
            "Type": "Add",
            "Parent": 5,
            "Instance": { "Id": 6, "Name": "Map", "ClassName": "Model", "Children": [{ "Id": 7, "Name": "Spawn", "ClassName": "SpawnLocation" }] }
        }
    ])"))));
    REQUIRE(findPath("Workspace/Map/Spawn"));
    CHECK(platform->virtualPathsToSourceNodes.count("game/Workspace/Map/Spawn") == 1);

    // Rename an instance
    REQUIRE(platform->onStudioPluginChanges(pluginChanges("session-1", json::parse(R"([{ "Type": "Update", "Id": 4, "Name": "Katana" }])"))));
    CHECK_FALSE(findPath("ReplicatedStorage/Assets/Sword"));
    REQUIRE(findPath("ReplicatedStorage/Assets/Katana"));
    CHECK(platform->virtualPathsToSourceNodes.count("game/ReplicatedStorage/Assets/Sword") == 0);

    // Reparent a subtree
    REQUIRE(platform->onStudioPluginChanges(pluginChanges("session-1", json::parse(R"([{ "Type": "Update", "Id": 3, "Parent": 6 }])"))));
    CHECK_FALSE(findPath("ReplicatedStorage/Assets"));
    REQUIRE(findPath("Workspace/Map/Assets/Katana"));

    // Remove a subtree
    REQUIRE(platform->onStudioPluginChanges(pluginChanges("session-1", json::parse(R"([{ "Type": "Remove", "Id": 6 }])"))));
    CHECK_FALSE(findPath("Workspace/Map"));
    CHECK(platform->virtualPathsToSourceNodes.count("game/Workspace/Map/Assets/Katana") == 0);
    REQUIRE(findPath("Workspace"));
}

TEST_CASE_FIXTURE(Fixture, "plugin_changes_update_file_paths")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    platform->onStudioPluginFullChange(kPluginDataModelWithIds);

    REQUIRE(platform->onStudioPluginChanges(pluginChanges("session-1", json::parse(R"([
        { "Type": "Add", "Parent": 3, "Instance": { "Id": 6, "Name": "Module", "ClassName": "ModuleScript" } },
        { "Type": "Update", "Id": 6, "FilePaths": ["src/Module.luau"] }
    ])"))));

    auto uri = workspace.rootUri.resolvePath("src/Module.luau");
    CHECK_EQ(platform->resolveToVirtualPath(uri), "game/ReplicatedStorage/Assets/Module");

    REQUIRE(platform->onStudioPluginChanges(pluginChanges("session-1", json::parse(R"([{ "Type": "Update", "Id": 6, "FilePaths": [] }])"))));
    CHECK_FALSE(platform->resolveToVirtualPath(uri));
}

TEST_CASE_FIXTURE(Fixture, "plugin_changes_provide_datamodel_types")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = true;
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    platform->onStudioPluginFullChange(kPluginDataModelWithIds);

    REQUIRE(platform->onStudioPluginChanges(pluginChanges("session-1", json::parse(R"([
        { "Type": "Add", "Parent": 5, "Instance": { "Id": 6, "Name": "Baseplate", "ClassName": "Part" } }
    ])"))));

    auto result = check(R"(
        --!strict
        local part = game:GetService("Workspace").Baseplate
        local tool = game:GetService("ReplicatedStorage").Assets.Sword
    )");
    LUAU_LSP_REQUIRE_NO_ERRORS(result);

    REQUIRE(platform->onStudioPluginChanges(pluginChanges("session-1", json::parse(R"([{ "Type": "Update", "Id": 6, "Name": "Floor" }])"))));

    auto result2 = check(R"(
        --!strict
        local part = game:GetService("Workspace").Baseplate
    )");
    REQUIRE_EQ(result2.errors.size(), 1);
    CHECK_EQ(Luau::get<Luau::UnknownProperty>(result2.errors[0])->key, "Baseplate");
}

TEST_CASE_FIXTURE(Fixture, "plugin_changes_require_a_matching_session")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    auto rename = json::parse(R"([{ "Type": "Update", "Id": 4, "Name": "Katana" }])");

    // No DataModel has been received yet (e.g. the language server restarted)
    CHECK_FALSE(platform->onStudioPluginChanges(pluginChanges("session-1", rename)));

    // DataModel from a plugin that does not support incremental changes
    auto legacyDataModel = kPluginDataModelWithIds;
    legacyDataModel.erase("Session");
    platform->onStudioPluginFullChange(legacyDataModel);
    CHECK_FALSE(platform->onStudioPluginChanges(pluginChanges("session-1", rename)));

    platform->onStudioPluginFullChange(kPluginDataModelWithIds);
    CHECK_FALSE(platform->onStudioPluginChanges(pluginChanges("session-2", rename)));
    CHECK_FALSE(platform->onStudioPluginChanges(json{{"changes", rename}}));
    CHECK(platform->onStudioPluginChanges(pluginChanges("session-1", rename)));

    // Clearing the DataModel invalidates the session
    platform->onStudioPluginClear();
    CHECK_FALSE(platform->onStudioPluginChanges(pluginChanges("session-1", rename)));
}

TEST_CASE_FIXTURE(Fixture, "plugin_changes_that_cannot_be_applied_require_a_full_resync")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    platform->onStudioPluginFullChange(kPluginDataModelWithIds);

    CHECK_FALSE(platform->onStudioPluginChanges(pluginChanges("session-1", json::parse(R"([{ "Type": "Remove", "Id": 999 }])"))));

    // The session is discarded, even for changes that would otherwise be valid
    CHECK_FALSE(platform->onStudioPluginChanges(pluginChanges("session-1", json::parse(R"([{ "Type": "Update", "Id": 4, "Name": "Katana" }])"))));

    platform->onStudioPluginFullChange(kPluginDataModelWithIds);
    CHECK(platform->onStudioPluginChanges(pluginChanges("session-1", json::parse(R"([{ "Type": "Update", "Id": 4, "Name": "Katana" }])"))));
}

TEST_CASE_FIXTURE(Fixture, "plugin_changes_reject_invalid_changes")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    for (const auto& invalidChange : {
             R"({ "Type": "Update", "Id": 2, "Parent": 3 })",                                                   // parent to own descendant
             R"({ "Type": "Update", "Id": 3, "Parent": 3 })",                                                   // parent to itself
             R"({ "Type": "Remove", "Id": 1 })",                                                                // remove the root
             R"({ "Type": "Add", "Parent": 5, "Instance": { "Id": 4, "Name": "Dupe", "ClassName": "Part" } })", // duplicate id
             R"({ "Type": "Add", "Parent": 5, "Instance": { "Name": "NoId", "ClassName": "Part" } })",          // missing id
             R"({ "Type": "Unknown", "Id": 2 })",
         })
    {
        platform->onStudioPluginFullChange(kPluginDataModelWithIds);
        CHECK_FALSE(platform->onStudioPluginChanges(pluginChanges("session-1", json::array({json::parse(invalidChange)}))));
    }
}

TEST_CASE_FIXTURE(Fixture, "plugin_malformed_full_datamodel_keeps_the_previous_datamodel")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    platform->onStudioPluginFullChange(kPluginDataModelWithIds);

    auto malformed = kPluginDataModelWithIds;
    malformed["Children"][0]["FilePaths"] = json::array({1});
    CHECK_THROWS(platform->onStudioPluginFullChange(malformed));

    CHECK(platform->onStudioPluginChanges(pluginChanges("session-1", json::parse(R"([{ "Type": "Update", "Id": 4, "Name": "Katana" }])"))));
    CHECK(platform->rootSourceNode->walkPath("ReplicatedStorage/Assets/Katana"));
}

TEST_CASE_FIXTURE(Fixture, "handle_request_routes_plugin_changes_request")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    platform->onStudioPluginFullChange(kPluginDataModelWithIds);

    auto result =
        platform->handleRequest("$/plugin/changes", pluginChanges("session-1", json::parse(R"([{ "Type": "Update", "Id": 4, "Name": "Katana" }])")));
    REQUIRE(result);
    CHECK(result->is_null());
    CHECK(platform->rootSourceNode->walkPath("ReplicatedStorage/Assets/Katana"));

    CHECK_THROWS_AS(platform->handleRequest("$/plugin/changes", pluginChanges("session-2", json::array())), JsonRpcException);
    CHECK_FALSE(platform->handleRequest("$/plugin/unknown", std::nullopt));
}

TEST_CASE_FIXTURE(Fixture, "plugin_changes_compact_removed_instances")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    platform->onStudioPluginFullChange(kPluginDataModelWithIds);

    uint64_t nextId = 100;
    for (int i = 0; i < 100; i++)
    {
        json children = json::array();
        for (int j = 0; j < 100; j++)
            children.push_back({{"Id", nextId++}, {"Name", "Part" + std::to_string(j)}, {"ClassName", "Part"}});
        uint64_t folderId = nextId++;
        json changes = json::array({
            {{"Type", "Add"}, {"Parent", 5}, {"Instance", {{"Id", folderId}, {"Name", "Folder"}, {"ClassName", "Folder"}, {"Children", children}}}},
        });
        REQUIRE(platform->onStudioPluginChanges(pluginChanges("session-1", changes)));
        REQUIRE(platform->onStudioPluginChanges(pluginChanges("session-1", json::array({{{"Type", "Remove"}, {"Id", folderId}}}))));
    }

    // 10,000 instances were added and removed, but only the 5 live instances should remain allocated after compaction
    CHECK_LT(platform->pluginNodeAllocator.size(), 4096);
    CHECK(platform->onStudioPluginChanges(pluginChanges("session-1", json::parse(R"([{ "Type": "Update", "Id": 4, "Name": "Katana" }])"))));
    CHECK(platform->rootSourceNode->walkPath("ReplicatedStorage/Assets/Katana"));
}

namespace
{
/// A model of the Studio DataModel, used to verify that incremental changes produce the same result as a full DataModel
struct PluginModelNode
{
    uint64_t id;
    std::string name;
    std::string className;
    std::vector<std::string> filePaths{};
    std::vector<std::unique_ptr<PluginModelNode>> children{};
    PluginModelNode* parent = nullptr;

    json toJson() const
    {
        json childrenJson = json::array();
        for (const auto& child : children)
            childrenJson.push_back(child->toJson());
        return {{"Id", id}, {"Name", name}, {"ClassName", className}, {"FilePaths", filePaths}, {"Children", childrenJson}};
    }

    void collect(std::vector<PluginModelNode*>& nodes)
    {
        nodes.push_back(this);
        for (auto& child : children)
            child->collect(nodes);
    }

    bool isAncestorOf(const PluginModelNode* other) const
    {
        for (auto node = other->parent; node; node = node->parent)
            if (node == this)
                return true;
        return false;
    }

    std::unique_ptr<PluginModelNode> detach()
    {
        auto& siblings = parent->children;
        auto it = std::find_if(siblings.begin(), siblings.end(),
            [this](const auto& child)
            {
                return child.get() == this;
            });
        auto self = std::move(*it);
        siblings.erase(it);
        parent = nullptr;
        return self;
    }

    void append(std::unique_ptr<PluginModelNode> child)
    {
        child->parent = this;
        children.push_back(std::move(child));
    }
};
} // namespace

TEST_CASE_FIXTURE(Fixture, "plugin_changes_are_equivalent_to_sending_the_full_datamodel")
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());

    std::mt19937 rng(1234);
    auto randomInt = [&](size_t max) -> size_t
    {
        return std::uniform_int_distribution<size_t>(0, max - 1)(rng);
    };
    // A small pool of names, so that duplicate sibling names are exercised
    const std::vector<std::string> names{"A", "B", "C", "Part", "Folder"};
    const std::vector<std::string> classNames{"Folder", "Model", "Part", "ModuleScript"};
    uint64_t nextId = 1;

    auto makeNode = [&]()
    {
        auto node =
            std::make_unique<PluginModelNode>(PluginModelNode{nextId++, names[randomInt(names.size())], classNames[randomInt(classNames.size())]});
        if (node->className == "ModuleScript" && randomInt(2) == 0)
            node->filePaths = {"src/" + std::to_string(node->id) + ".luau"};
        return node;
    };

    PluginModelNode root{nextId++, "game", "DataModel"};
    for (int i = 0; i < 20; i++)
    {
        std::vector<PluginModelNode*> nodes;
        root.collect(nodes);
        nodes[randomInt(nodes.size())]->append(makeNode());
    }

    int sessionNumber = 0;
    auto sendFullDataModel = [&]()
    {
        auto dataModel = root.toJson();
        dataModel["Session"] = "session-" + std::to_string(++sessionNumber);
        platform->onStudioPluginFullChange(dataModel);
    };
    sendFullDataModel();

    for (int batch = 0; batch < 300; batch++)
    {
        json changes = json::array();
        size_t changeCount = 1 + randomInt(8);
        for (size_t i = 0; i < changeCount; i++)
        {
            std::vector<PluginModelNode*> nodes;
            root.collect(nodes);
            auto node = nodes[randomInt(nodes.size())];
            auto changeType = randomInt(5);
            if (node == &root && changeType != 0)
                continue; // Only children can be added to the root
            switch (changeType)
            {
            case 0:
            {
                auto child = makeNode();
                for (size_t j = randomInt(3); j > 0; j--)
                    child->append(makeNode());
                changes.push_back({{"Type", "Add"}, {"Parent", node->id}, {"Instance", child->toJson()}});
                node->append(std::move(child));
                break;
            }
            case 1:
                if (nodes.size() > 10)
                {
                    changes.push_back({{"Type", "Remove"}, {"Id", node->id}});
                    node->detach();
                }
                break;
            case 2:
                node->name = names[randomInt(names.size())];
                changes.push_back({{"Type", "Update"}, {"Id", node->id}, {"Name", node->name}});
                break;
            case 3:
            {
                auto newParent = nodes[randomInt(nodes.size())];
                if (newParent == node || node->isAncestorOf(newParent))
                    break;
                json change{{"Type", "Update"}, {"Id", node->id}, {"Parent", newParent->id}};
                if (randomInt(2) == 0)
                {
                    node->name = names[randomInt(names.size())];
                    change["Name"] = node->name;
                }
                changes.push_back(change);
                newParent->append(node->detach());
                break;
            }
            case 4:
                node->filePaths =
                    randomInt(2) == 0 ? std::vector<std::string>{} : std::vector<std::string>{"src/moved" + std::to_string(batch) + ".luau"};
                changes.push_back({{"Type", "Update"}, {"Id", node->id}, {"FilePaths", node->filePaths}});
                break;
            }
        }

        REQUIRE(platform->onStudioPluginChanges(pluginChanges("session-" + std::to_string(sessionNumber), changes)));
        auto incremental = platform->rootSourceNode->toJson();
        auto incrementalPaths = platform->virtualPathsToSourceNodes.size();

        // Hydrating with the full DataModel must not change anything
        sendFullDataModel();
        CHECK_EQ(incremental, platform->rootSourceNode->toJson());
        CHECK_EQ(incrementalPaths, platform->virtualPathsToSourceNodes.size());
    }
}

TEST_SUITE_END();
