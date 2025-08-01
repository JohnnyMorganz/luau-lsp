#include "doctest.h"
#include "Fixture.h"
#include "Platform/RobloxPlatform.hpp"
#include "ScopedFlags.h"

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
    ScopedFastFlag sff{FFlag::LuauSolverV2, true};

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

TEST_SUITE_END();
