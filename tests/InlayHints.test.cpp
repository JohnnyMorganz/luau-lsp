#include "doctest.h"
#include "Fixture.h"
#include "LSP/IostreamHelpers.hpp"
#include "Luau/Common.h"

LUAU_FASTFLAG(LuauSolverV2)

TEST_SUITE_BEGIN("InlayHints");

static std::string labelToString(const std::vector<lsp::InlayHintLabelPart>& parts)
{
    std::string result;
    for (const auto& part : parts)
        result += part.value;
    return result;
}

static lsp::InlayHintResult processInlayHint(Fixture* fixture, const std::string source)
{
    auto uri = fixture->newDocument("foo.luau", source);
    lsp::InlayHintParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    return fixture->workspace.inlayHint(params, /* cancellationToken= */ nullptr);
}

TEST_CASE_FIXTURE(Fixture, "show_inlay_hint_on_local_definition")
{
    client->globalConfig.inlayHints.variableTypes = true;
    auto source = R"(
        local x = 1
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{1, 15});
    CHECK_EQ(labelToString(result[0].label), ": number");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);

    REQUIRE(result[0].textEdits.size() == 1);
    CHECK_EQ(result[0].textEdits[0].newText, ": number");
    CHECK_EQ(result[0].textEdits[0].range, lsp::Range{{1, 15}, {1, 15}});
}

TEST_CASE_FIXTURE(Fixture, "show_inlay_hint_on_multiple_local_definition")
{
    client->globalConfig.inlayHints.variableTypes = true;
    auto source = R"(
        local x, y = 1, "foo"
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 2);

    CHECK_EQ(result[0].position, lsp::Position{1, 15});
    CHECK_EQ(labelToString(result[0].label), ": number");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);

    REQUIRE(result[0].textEdits.size() == 1);
    CHECK_EQ(result[0].textEdits[0].newText, ": number");
    CHECK_EQ(result[0].textEdits[0].range, lsp::Range{{1, 15}, {1, 15}});

    CHECK_EQ(result[1].position, lsp::Position{1, 18});
    CHECK_EQ(labelToString(result[1].label), ": string");
    CHECK_EQ(result[1].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[1].tooltip, std::nullopt);
    CHECK_EQ(result[1].paddingLeft, false);
    CHECK_EQ(result[1].paddingRight, false);

    REQUIRE(result[1].textEdits.size() == 1);
    CHECK_EQ(result[1].textEdits[0].newText, ": string");
    CHECK_EQ(result[1].textEdits[0].range, lsp::Range{{1, 18}, {1, 18}});
}

TEST_CASE_FIXTURE(Fixture, "hide_inlay_hint_on_local_definition_with_type_annotation")
{
    client->globalConfig.inlayHints.variableTypes = true;
    auto source = R"(
        local x: number = 1
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "hide_inlay_hints_on_underscore_local_definition")
{
    client->globalConfig.inlayHints.variableTypes = true;
    auto source = R"(
        local _ = 1
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "hide_inlay_hints_on_local_definition_of_function")
{
    client->globalConfig.inlayHints.variableTypes = true;
    auto source = R"(
        local x = function()
            return 1
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "hide_inlay_hints_when_local_definition_name_matches_type_name")
{
    client->globalConfig.inlayHints.variableTypes = true;
    auto source = R"(
        type Foo = { x: number }
        local _: Foo = { x = 1 }
        local Foo = _
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "respect_variable_types_configuration_local_definition")
{
    client->globalConfig.inlayHints.variableTypes = false;
    auto source = R"(
        local x = 1
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "respect_variable_types_make_insertable_configuration_local_definition")
{
    client->globalConfig.inlayHints.variableTypes = true;
    client->globalConfig.inlayHints.makeInsertable = false;
    auto source = R"(
        local x = 1
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{1, 15});
    CHECK_EQ(labelToString(result[0].label), ": number");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);
    CHECK_EQ(result[0].textEdits.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "show_inlay_hint_for_error_types_local_definition")
{
    client->globalConfig.inlayHints.variableTypes = true;
    auto source = R"(
        local x = 1 :: Unknown
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{1, 15});
    CHECK_EQ(labelToString(result[0].label), ": *error-type*");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);
    CHECK_EQ(result[0].textEdits.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "respect_hide_inlay_hints_for_error_type_configuration_local_definition")
{
    client->globalConfig.inlayHints.variableTypes = true;
    client->globalConfig.inlayHints.hideHintsForErrorTypes = true;
    auto source = R"(
        local x = 1 :: Unknown
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "show_inlay_hint_in_for_loop")
{
    client->globalConfig.inlayHints.variableTypes = true;
    auto source = R"(
        local _ = { "foo" }

        for i, v in pairs(_) do
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 2);

    CHECK_EQ(result[0].position, lsp::Position{3, 13});
    CHECK_EQ(labelToString(result[0].label), ": number");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);

    REQUIRE(result[0].textEdits.size() == 1);
    CHECK_EQ(result[0].textEdits[0].newText, ": number");
    CHECK_EQ(result[0].textEdits[0].range, lsp::Range{{3, 13}, {3, 13}});

    CHECK_EQ(result[1].position, lsp::Position{3, 16});
    CHECK_EQ(labelToString(result[1].label), ": string");
    CHECK_EQ(result[1].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[1].tooltip, std::nullopt);
    CHECK_EQ(result[1].paddingLeft, false);
    CHECK_EQ(result[1].paddingRight, false);

    REQUIRE(result[1].textEdits.size() == 1);
    CHECK_EQ(result[1].textEdits[0].newText, ": string");
    CHECK_EQ(result[1].textEdits[0].range, lsp::Range{{3, 16}, {3, 16}});
}

TEST_CASE_FIXTURE(Fixture, "hide_inlay_hint_in_for_loop_with_type_annotation")
{
    client->globalConfig.inlayHints.variableTypes = true;
    auto source = R"(
        local _ = { "foo" }

        for i: number, v: string in pairs(_) do
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "hide_inlay_hints_on_underscore_in_for_loop")
{
    client->globalConfig.inlayHints.variableTypes = true;
    auto source = R"(
        local _ = { "foo" }

        for _, v in pairs(_) do
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{3, 16});
    CHECK_EQ(labelToString(result[0].label), ": string");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);

    REQUIRE(result[0].textEdits.size() == 1);
    CHECK_EQ(result[0].textEdits[0].newText, ": string");
    CHECK_EQ(result[0].textEdits[0].range, lsp::Range{{3, 16}, {3, 16}});
}

TEST_CASE_FIXTURE(Fixture, "hide_inlay_hints_when_for_loop_variable_name_matches_type_name")
{
    client->globalConfig.inlayHints.variableTypes = true;
    auto source = R"(
        local _ = { "foo" }

        for number, string in pairs(_) do
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "respect_variable_types_configuration_for_loop")
{
    client->globalConfig.inlayHints.variableTypes = false;
    auto source = R"(
        local _ = { "foo" }

        for i, v in pairs(_) do
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "respect_variable_types_make_insertable_configuration_for_loop")
{
    client->globalConfig.inlayHints.variableTypes = true;
    client->globalConfig.inlayHints.makeInsertable = false;
    auto source = R"(
        local _ = { "foo" }

        for i, v in pairs(_) do
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 2);

    CHECK_EQ(result[0].position, lsp::Position{3, 13});
    CHECK_EQ(labelToString(result[0].label), ": number");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);
    CHECK_EQ(result[0].textEdits.size(), 0);

    CHECK_EQ(result[1].position, lsp::Position{3, 16});
    CHECK_EQ(labelToString(result[1].label), ": string");
    CHECK_EQ(result[1].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[1].tooltip, std::nullopt);
    CHECK_EQ(result[1].paddingLeft, false);
    CHECK_EQ(result[1].paddingRight, false);
    CHECK_EQ(result[1].textEdits.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "show_inlay_hint_for_error_types_for_loop")
{
    client->globalConfig.inlayHints.variableTypes = true;
    auto source = R"(
        for i,v in UNKNOWN do
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 2);

    CHECK_EQ(result[0].position, lsp::Position{1, 13});
    CHECK_EQ(labelToString(result[0].label), ": *error-type*");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);
    CHECK_EQ(result[0].textEdits.size(), 0);

    CHECK_EQ(result[1].position, lsp::Position{1, 15});
    CHECK_EQ(labelToString(result[1].label), ": *error-type*");
    CHECK_EQ(result[1].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[1].tooltip, std::nullopt);
    CHECK_EQ(result[1].paddingLeft, false);
    CHECK_EQ(result[1].paddingRight, false);
    CHECK_EQ(result[1].textEdits.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "respect_hide_inlay_hints_for_error_type_configuration_for_loop")
{
    client->globalConfig.inlayHints.variableTypes = true;
    client->globalConfig.inlayHints.hideHintsForErrorTypes = true;
    auto source = R"(
        for i,v in UNKNOWN do
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "show_inlay_hint_for_parameter_type")
{
    client->globalConfig.inlayHints.parameterTypes = true;
    auto source = R"(
        local function id(_: string) end

        local function foo(x)
            id(x)
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{3, 28});
    CHECK_EQ(labelToString(result[0].label), ": string");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);

    REQUIRE(result[0].textEdits.size() == 1);
    CHECK_EQ(result[0].textEdits[0].newText, ": string");
    CHECK_EQ(result[0].textEdits[0].range, lsp::Range{{3, 28}, {3, 28}});
}

TEST_CASE_FIXTURE(Fixture, "hide_inlay_hint_for_parameter_with_type_annotation")
{
    client->globalConfig.inlayHints.parameterTypes = true;
    auto source = R"(
        local function id(x: number)
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "hide_inlay_hint_for_underscore_parameter")
{
    client->globalConfig.inlayHints.parameterTypes = true;
    auto source = R"(
        local function id(_)
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "handle_parameter_types_inlay_hint_for_method")
{
    client->globalConfig.inlayHints.parameterTypes = true;
    auto source = R"(
        local _ = { data = 1 }

        local function id(_: string) end

        function _:Foo(x)
            id(x)
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{5, 24});
    CHECK_EQ(labelToString(result[0].label), ": string");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);

    REQUIRE(result[0].textEdits.size() == 1);
    CHECK_EQ(result[0].textEdits[0].newText, ": string");
    CHECK_EQ(result[0].textEdits[0].range, lsp::Range{{5, 24}, {5, 24}});
}

TEST_CASE_FIXTURE(Fixture, "handle_parameter_types_inlay_hint_for_vararg")
{
    client->globalConfig.inlayHints.parameterTypes = true;
    auto source = R"(
        local function id(...)
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{1, 29});
    CHECK_EQ(labelToString(result[0].label), ": any");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);

    REQUIRE(result[0].textEdits.size() == 1);
    CHECK_EQ(result[0].textEdits[0].newText, ": any");
    CHECK_EQ(result[0].textEdits[0].range, lsp::Range{{1, 29}, {1, 29}});
}

TEST_CASE_FIXTURE(Fixture, "handle_parameter_types_inlay_hint_for_vararg_2")
{
    client->globalConfig.inlayHints.parameterTypes = true;
    auto source = R"(
        local function id(x: string, ...)
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{1, 40});
    CHECK_EQ(labelToString(result[0].label), ": any");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);

    REQUIRE(result[0].textEdits.size() == 1);
    CHECK_EQ(result[0].textEdits[0].newText, ": any");
    CHECK_EQ(result[0].textEdits[0].range, lsp::Range{{1, 40}, {1, 40}});
}

TEST_CASE_FIXTURE(Fixture, "hide_inlay_hint_on_vararg_with_type_annotation")
{
    client->globalConfig.inlayHints.parameterTypes = true;
    auto source = R"(
        local function id(...: string)
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "respect_parameter_types_configuration")
{
    client->globalConfig.inlayHints.parameterTypes = false;
    auto source = R"(
        local function foo(x)
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "respect_parameter_types_make_insertable_configuration")
{
    client->globalConfig.inlayHints.parameterTypes = true;
    client->globalConfig.inlayHints.makeInsertable = false;
    auto source = R"(
        local function id(_: string) end

        local function foo(x)
            id(x)
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{3, 28});
    CHECK_EQ(labelToString(result[0].label), ": string");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);
    CHECK_EQ(result[0].textEdits.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "show_inlay_hint_for_return_type")
{
    client->globalConfig.inlayHints.functionReturnTypes = true;
    auto source = R"(
        local function example(value1: number, value2: number)
            return value1 + value2
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{1, 62});
    CHECK_EQ(labelToString(result[0].label), ": number");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);

    REQUIRE(result[0].textEdits.size() == 1);
    CHECK_EQ(result[0].textEdits[0].newText, ": number");
    CHECK_EQ(result[0].textEdits[0].range, lsp::Range{{1, 62}, {1, 62}});
}

TEST_CASE_FIXTURE(Fixture, "respect_function_return_type_configuration")
{
    client->globalConfig.inlayHints.functionReturnTypes = false;
    auto source = R"(
        local function example(value1: number, value2: number)
            return value1 + value2
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "respect_function_return_type_make_insertable_configuration")
{
    client->globalConfig.inlayHints.functionReturnTypes = true;
    client->globalConfig.inlayHints.makeInsertable = false;
    auto source = R"(
        local function example(value1: number, value2: number)
            return value1 + value2
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{1, 62});
    CHECK_EQ(labelToString(result[0].label), ": number");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);
    CHECK_EQ(result[0].textEdits.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "show_inlay_hint_for_parameter_name_in_call_with_literal_expr")
{
    client->globalConfig.inlayHints.parameterNames = InlayHintsParameterNamesConfig::All;
    auto source = R"(
        local function id(value: string)
        end

        id("testing")
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{4, 11});
    CHECK_EQ(labelToString(result[0].label), "value:");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Parameter);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, true);
    CHECK_EQ(result[0].textEdits.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "show_inlay_hint_for_parameter_name_in_call_with_variable_expr")
{
    client->globalConfig.inlayHints.parameterNames = InlayHintsParameterNamesConfig::All;
    auto source = R"(
        local function id(value: string)
        end

        local data = "testing"
        id(data)
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{5, 11});
    CHECK_EQ(labelToString(result[0].label), "value:");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Parameter);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, true);
    CHECK_EQ(result[0].textEdits.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "hide_inlay_hint_for_underscore_parameter_name")
{
    client->globalConfig.inlayHints.parameterNames = InlayHintsParameterNamesConfig::All;
    auto source = R"(
        local function id(_: string)
        end

        id("testing")
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "hide_inlay_hint_if_variable_matches_parameter_name")
{
    client->globalConfig.inlayHints.parameterNames = InlayHintsParameterNamesConfig::All;
    auto source = R"(
        local function id(value: string)
        end

        local value = "testing"
        id(value)
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "show_inlay_hint_if_variable_matches_parameter_name_and_hidden_configuration_is_disabled")
{
    client->globalConfig.inlayHints.parameterNames = InlayHintsParameterNamesConfig::All;
    client->globalConfig.inlayHints.hideHintsForMatchingParameterNames = false;
    auto source = R"(
        local function id(value: string)
        end

        local value = "testing"
        id(value)
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{5, 11});
    CHECK_EQ(labelToString(result[0].label), "value:");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Parameter);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, true);
    CHECK_EQ(result[0].textEdits.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "hide_inlay_hint_if_indexed_expression_matches_parameter_name")
{
    client->globalConfig.inlayHints.parameterNames = InlayHintsParameterNamesConfig::All;
    auto source = R"(
        local function id(value: string)
        end

        local _ = { value = "testing"}
        id(_.value)
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "show_inlay_hint_if_indexed_expression_matches_parameter_name_and_hidden_configuration_is_disabled")
{
    client->globalConfig.inlayHints.parameterNames = InlayHintsParameterNamesConfig::All;
    client->globalConfig.inlayHints.hideHintsForMatchingParameterNames = false;
    auto source = R"(
        local function id(value: string)
        end

        local _ = { value = "testing"}
        id(_.value)
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{5, 11});
    CHECK_EQ(labelToString(result[0].label), "value:");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Parameter);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, true);
    CHECK_EQ(result[0].textEdits.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "only_show_parameter_name_for_literal_based_on_configuration")
{
    client->globalConfig.inlayHints.parameterNames = InlayHintsParameterNamesConfig::Literals;
    auto source = R"(
        local function id(value1: string, value2: string)
        end

        local data = "testing"
        id(data, "another")
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{5, 17});
    CHECK_EQ(labelToString(result[0].label), "value2:");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Parameter);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, true);
    CHECK_EQ(result[0].textEdits.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "dont_show_inlay_hint_for_self_type_when_calling_with_colon")
{
    client->globalConfig.inlayHints.parameterNames = InlayHintsParameterNamesConfig::All;
    auto source = R"(
        local t = {}
        function t.id(t, value)
        end

        t:id("testing")
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{5, 13});
    CHECK_EQ(labelToString(result[0].label), "value:");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Parameter);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, true);
    CHECK_EQ(result[0].textEdits.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "show_inlay_hint_for_self_type_when_calling_with_dot")
{
    client->globalConfig.inlayHints.parameterNames = InlayHintsParameterNamesConfig::All;
    auto source = R"(
        local t = {}
        function t:id(value)
        end

        t.id(t, "testing")
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 2);

    CHECK_EQ(result[0].position, lsp::Position{5, 13});
    CHECK_EQ(labelToString(result[0].label), "self:");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Parameter);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, true);
    CHECK_EQ(result[0].textEdits.size(), 0);

    CHECK_EQ(result[1].position, lsp::Position{5, 16});
    CHECK_EQ(labelToString(result[1].label), "value:");
    CHECK_EQ(result[1].kind, lsp::InlayHintKind::Parameter);
    CHECK_EQ(result[1].tooltip, std::nullopt);
    CHECK_EQ(result[1].paddingLeft, false);
    CHECK_EQ(result[1].paddingRight, true);
    CHECK_EQ(result[1].textEdits.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "respect_parameter_names_configuration")
{
    client->globalConfig.inlayHints.parameterNames = InlayHintsParameterNamesConfig::None;
    auto source = R"(
        local function id(value1: string, value2: string)
        end

        local data = "testing"
        id(data, "another")
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "skip_self_as_first_parameter_on_method_definitions")
{
    client->globalConfig.inlayHints.parameterTypes = true;
    auto source = R"(
        local Class = {}
        function Class:foo(bar)
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{2, 30});
    if (FFlag::LuauSolverV2)
        CHECK_EQ(labelToString(result[0].label), ": unknown");
    else
        CHECK_EQ(labelToString(result[0].label), ": a");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);

    REQUIRE(result[0].textEdits.size() == 1);
    if (FFlag::LuauSolverV2)
        CHECK_EQ(result[0].textEdits[0].newText, ": unknown");
    else
        CHECK_EQ(result[0].textEdits[0].newText, ": a");
    CHECK_EQ(result[0].textEdits[0].range, lsp::Range{{2, 30}, {2, 30}});
}

TEST_CASE_FIXTURE(Fixture, "skip_self_as_first_parameter_on_method_definitions_2")
{
    // TODO: New solver bug - https://github.com/luau-lang/luau/issues/1516
    if (FFlag::LuauSolverV2)
        return;

    client->globalConfig.inlayHints.parameterTypes = true;
    auto source = R"(
        type Class = {
            foo: (self: Class, bar: string) -> ()
        }
        local Class = {} :: Class

        function Class:foo(bar)
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    CHECK_EQ(result[0].position, lsp::Position{6, 30});
    CHECK_EQ(labelToString(result[0].label), ": string");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);

    REQUIRE(result[0].textEdits.size() == 1);
    CHECK_EQ(result[0].textEdits[0].newText, ": string");
    CHECK_EQ(result[0].textEdits[0].range, lsp::Range{{6, 30}, {6, 30}});
}

TEST_CASE_FIXTURE(Fixture, "dont_skip_self_as_first_parameter_when_using_plain_function_definitions")
{
    // TODO: New solver bug - https://github.com/luau-lang/luau/issues/1516
    if (FFlag::LuauSolverV2)
        return;

    client->globalConfig.inlayHints.parameterTypes = true;
    auto source = R"(
        type Class = {
            foo: (self: Class, bar: string) -> ()
        }
        local Class = {} :: Class

        function Class.foo(self, bar)
        end
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 2);

    CHECK_EQ(result[0].position, lsp::Position{6, 31});
    CHECK_EQ(labelToString(result[0].label), ": Class");
    CHECK_EQ(result[0].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[0].tooltip, std::nullopt);
    CHECK_EQ(result[0].paddingLeft, false);
    CHECK_EQ(result[0].paddingRight, false);

    REQUIRE(result[0].textEdits.size() == 1);
    CHECK_EQ(result[0].textEdits[0].newText, ": Class");
    CHECK_EQ(result[0].textEdits[0].range, lsp::Range{{6, 31}, {6, 31}});

    CHECK_EQ(result[1].position, lsp::Position{6, 36});
    CHECK_EQ(labelToString(result[1].label), ": string");
    CHECK_EQ(result[1].kind, lsp::InlayHintKind::Type);
    CHECK_EQ(result[1].tooltip, std::nullopt);
    CHECK_EQ(result[1].paddingLeft, false);
    CHECK_EQ(result[1].paddingRight, false);

    REQUIRE(result[1].textEdits.size() == 1);
    CHECK_EQ(result[1].textEdits[0].newText, ": string");
    CHECK_EQ(result[1].textEdits[0].range, lsp::Range{{6, 36}, {6, 36}});
}

TEST_CASE_FIXTURE(Fixture, "inlay_hints_respects_cancellation")
{
    auto cancellationToken = std::make_shared<Luau::FrontendCancellationToken>();
    cancellationToken->cancel();

    auto document = newDocument("a.luau", "local x = 1");
    CHECK_THROWS_AS(workspace.inlayHint(lsp::InlayHintParams{{{document}}}, cancellationToken), RequestCancelledException);
}

TEST_CASE_FIXTURE(Fixture, "inlay_hint_label_uses_parts")
{
    client->globalConfig.inlayHints.variableTypes = true;
    auto source = R"(
        local x = 1
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    REQUIRE_GE(result[0].label.size(), 2);
    CHECK_EQ(result[0].label[0].value, ": ");
    CHECK_EQ(result[0].label[1].value, "number");
    CHECK_EQ(result[0].label[1].location, std::nullopt); // Primitive types don't have location
}

TEST_CASE_FIXTURE(Fixture, "inlay_hint_custom_type_has_separate_part")
{
    client->globalConfig.inlayHints.variableTypes = true;
    auto source = R"(
        type Player = { name: string }
        local p: Player = { name = "test" }
        local x = p
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    REQUIRE_EQ(result[0].label.size(), 2);
    CHECK_EQ(result[0].label[0].value, ": ");
    CHECK_EQ(result[0].label[1].value, "Player");
    REQUIRE(result[0].label[1].location);
    CHECK_EQ(result[0].label[1].location->range, lsp::Range{{1, 22}, {1, 38}});
}

TEST_CASE_FIXTURE(Fixture, "inlay_hint_union_type_of_primitives_has_no_parts")
{
    client->globalConfig.inlayHints.variableTypes = true;
    auto source = R"(
        local x: string | number = "test"
        local y = x
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    REQUIRE_EQ(result[0].label.size(), 2);
    CHECK_EQ(result[0].label[0].value, ": ");
    CHECK_EQ(result[0].label[1].value, "number | string");
}

TEST_CASE_FIXTURE(Fixture, "inlay_hint_union_with_custom_types_has_separate_parts")
{
    client->globalConfig.inlayHints.variableTypes = true;
    auto source = R"(
        type Foo = { x: number }
        type Bar = { y: string }
        local x: Foo | Bar = { x = 1 }
        local y = x
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    REQUIRE_EQ(result[0].label.size(), 4);
    CHECK_EQ(result[0].label[0].value, ": ");
    CHECK_EQ(result[0].label[1].value, "Bar");
    REQUIRE(result[0].label[1].location);
    CHECK_EQ(result[0].label[1].location->range, lsp::Range{{2, 19}, {2, 32}});
    CHECK_EQ(result[0].label[2].value, " | ");
    CHECK_EQ(result[0].label[3].value, "Foo");
    REQUIRE(result[0].label[3].location);
    CHECK_EQ(result[0].label[3].location->range, lsp::Range{{1, 19}, {1, 32}});
}

TEST_CASE_FIXTURE(Fixture, "inlay_hint_generics_and_extern_type")
{
    client->globalConfig.inlayHints.variableTypes = true;
    auto source = R"(
        type Box<T> = { inner: T }
        local x: Box<Instance>
        local y = x
    )";

    auto result = processInlayHint(this, source);
    REQUIRE_EQ(result.size(), 1);

    REQUIRE_EQ(result[0].label.size(), 5);
    CHECK_EQ(result[0].label[0].value, ": ");
    CHECK_EQ(result[0].label[1].value, "Box");
    REQUIRE(result[0].label[1].location);
    CHECK_EQ(result[0].label[1].location->range, lsp::Range{{2, 17}, {2, 30}});
    CHECK_EQ(result[0].label[2].value, "<");
    CHECK_EQ(result[0].label[3].value, "Instance");
    REQUIRE(result[0].label[3].location);
    CHECK_EQ(result[0].label[3].location->range, lsp::Range{{1, 13}, {1, 16}});
    CHECK_EQ(result[0].label[4].value, ">");
}

TEST_SUITE_END();
