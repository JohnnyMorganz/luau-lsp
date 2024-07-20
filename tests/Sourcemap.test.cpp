#include "doctest.h"
#include "Fixture.h"
#include "Platform/RobloxPlatform.hpp"

TEST_SUITE_BEGIN("SourcemapTests");

TEST_CASE("getScriptFilePath")
{
    SourceNode node;
    node.className = "ModuleScript";
    node.filePaths = {"test.lua"};
    CHECK_EQ(node.getScriptFilePath(), "test.lua");
}

TEST_CASE("getScriptFilePath returns json file if node is populated by JSON")
{
    SourceNode node;
    node.className = "ModuleScript";
    node.filePaths = {"test.json"};
    CHECK_EQ(node.getScriptFilePath(), "test.json");
}

TEST_CASE("getScriptFilePath returns toml file if node is populated by TOML")
{
    SourceNode node;
    node.className = "ModuleScript";
    node.filePaths = {"test.toml"};
    CHECK_EQ(node.getScriptFilePath(), "test.toml");
}

TEST_CASE("getScriptFilePath doesn't pick .meta.json")
{
    SourceNode node;
    node.className = "ModuleScript";
    node.filePaths = {"init.meta.json", "init.lua"};
    CHECK_EQ(node.getScriptFilePath(), "init.lua");
}

TEST_CASE_FIXTURE(Fixture, "datamodel_entry_points_typed_as_any_when_strict_datamodel_types_is_off_no_sourcemap")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = false;
    auto result = check(R"(
        --!strict
        local x = game
        local y = workspace
        local z = script
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("x")) == "any");
    CHECK(Luau::toString(requireType("y")) == "any");
    CHECK(Luau::toString(requireType("z")) == "any");
}

TEST_CASE_FIXTURE(Fixture, "datamodel_entry_points_typed_as_any_when_strict_datamodel_types_is_off_with_sourcemap")
{
    client->globalConfig.diagnostics.strictDatamodelTypes = false;
    loadSourcemap(R"({"name": "Game", "className": "DataModel", "children": []})");
    auto result = check(R"(
        --!strict
        local x = game
        local y = workspace
        local z = script
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("x")) == "any");
    CHECK(Luau::toString(requireType("y")) == "any");
    CHECK(Luau::toString(requireType("z")) == "any");
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


TEST_CASE_FIXTURE(Fixture, "find_first_child_still_supports_recursive_parameter_with_sourcemap")
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

    // TODO: Support recursive datamodel lookup - https://github.com/JohnnyMorganz/luau-lsp/issues/689
    auto result = check(R"(
        --!strict
        local template = game:FindFirstChild("Head", true)
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("template")) == "Instance?");
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


TEST_CASE_FIXTURE(Fixture, "find_first_child_finds_direct_child_recursively")
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

TEST_SUITE_END();
