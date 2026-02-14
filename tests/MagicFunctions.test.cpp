#include "doctest.h"
#include "Fixture.h"
#include "Platform/RobloxPlatform.hpp"

LUAU_FASTFLAG(LuauTypeCheckerUdtfRenameClassToExtern)

TEST_SUITE_BEGIN("SelectorParser");

TEST_CASE("parse_selector_simple_class")
{
    auto result = parseClassNamesFromSelector("BasePart");
    REQUIRE(result.size() == 1);
    CHECK(result[0] == "BasePart");
}

TEST_CASE("parse_selector_tag_only")
{
    auto result = parseClassNamesFromSelector(".Tag");
    CHECK(result.empty());
}

TEST_CASE("parse_selector_name_only")
{
    auto result = parseClassNamesFromSelector("#MyName");
    CHECK(result.empty());
}

TEST_CASE("parse_selector_property_only")
{
    auto result = parseClassNamesFromSelector("[Anchored = true]");
    CHECK(result.empty());
}

TEST_CASE("parse_selector_attribute_only")
{
    auto result = parseClassNamesFromSelector("[$FuelCapacity]");
    CHECK(result.empty());
}

TEST_CASE("parse_selector_compound_class_tag")
{
    auto result = parseClassNamesFromSelector("BasePart.Tag");
    REQUIRE(result.size() == 1);
    CHECK(result[0] == "BasePart");
}

TEST_CASE("parse_selector_compound_class_property")
{
    auto result = parseClassNamesFromSelector("MeshPart[Anchored = false]");
    REQUIRE(result.size() == 1);
    CHECK(result[0] == "MeshPart");
}

TEST_CASE("parse_selector_descendant_combinator")
{
    auto result = parseClassNamesFromSelector("Model >> BasePart");
    REQUIRE(result.size() == 1);
    CHECK(result[0] == "BasePart");
}

TEST_CASE("parse_selector_child_combinator")
{
    auto result = parseClassNamesFromSelector("Model > Part");
    REQUIRE(result.size() == 1);
    CHECK(result[0] == "Part");
}

TEST_CASE("parse_selector_leading_child_combinator")
{
    auto result = parseClassNamesFromSelector("> Part");
    REQUIRE(result.size() == 1);
    CHECK(result[0] == "Part");
}

TEST_CASE("parse_selector_grouping_two")
{
    auto result = parseClassNamesFromSelector("Part, TextLabel");
    REQUIRE(result.size() == 2);
    CHECK(result[0] == "Part");
    CHECK(result[1] == "TextLabel");
}

TEST_CASE("parse_selector_grouping_three")
{
    auto result = parseClassNamesFromSelector("Part, TextLabel, BasePart");
    REQUIRE(result.size() == 3);
    CHECK(result[0] == "Part");
    CHECK(result[1] == "TextLabel");
    CHECK(result[2] == "BasePart");
}

TEST_CASE("parse_selector_grouping_mixed")
{
    auto result = parseClassNamesFromSelector("Part, .Tag");
    REQUIRE(result.size() == 1);
    CHECK(result[0] == "Part");
}

TEST_CASE("parse_selector_not_pseudo")
{
    auto result = parseClassNamesFromSelector(":not(Part)");
    CHECK(result.empty());
}

TEST_CASE("parse_selector_class_with_not")
{
    auto result = parseClassNamesFromSelector("BasePart:not(Part)");
    REQUIRE(result.size() == 1);
    CHECK(result[0] == "BasePart");
}

TEST_CASE("parse_selector_class_with_has")
{
    auto result = parseClassNamesFromSelector("MeshPart:has(> .SwordPart)");
    REQUIRE(result.size() == 1);
    CHECK(result[0] == "MeshPart");
}

TEST_CASE("parse_selector_has_only")
{
    auto result = parseClassNamesFromSelector(":has(Tool)");
    CHECK(result.empty());
}

TEST_CASE("parse_selector_complex_chain")
{
    auto result = parseClassNamesFromSelector("Model >> BasePart:not(Part)");
    REQUIRE(result.size() == 1);
    CHECK(result[0] == "BasePart");
}

TEST_CASE("parse_selector_combinator_rhs_no_class")
{
    auto result = parseClassNamesFromSelector("Model >> [$OnFire = true]");
    CHECK(result.empty());
}

TEST_CASE("parse_selector_combinator_rhs_tag")
{
    auto result = parseClassNamesFromSelector("Model > .SwordPart");
    CHECK(result.empty());
}

TEST_CASE("parse_selector_combinator_rhs_name")
{
    auto result = parseClassNamesFromSelector("Model >> #RedTree");
    CHECK(result.empty());
}

TEST_CASE("parse_selector_empty_string")
{
    auto result = parseClassNamesFromSelector("");
    CHECK(result.empty());
}

TEST_CASE("parse_selector_whitespace_only")
{
    auto result = parseClassNamesFromSelector("  ");
    CHECK(result.empty());
}

TEST_CASE("parse_selector_nested_not_in_has")
{
    auto result = parseClassNamesFromSelector("Part:has(:not(.Tag))");
    REQUIRE(result.size() == 1);
    CHECK(result[0] == "Part");
}

TEST_CASE("parse_selector_unbalanced_open_paren")
{
    auto result = parseClassNamesFromSelector(":not(Part");
    CHECK(result.empty());
}

TEST_CASE("parse_selector_unbalanced_close_paren")
{
    auto result = parseClassNamesFromSelector("Part)");
    REQUIRE(result.size() == 1);
    CHECK(result[0] == "Part");
}

TEST_CASE("parse_selector_double_comma")
{
    auto result = parseClassNamesFromSelector("Part,,TextLabel");
    REQUIRE(result.size() == 2);
    CHECK(result[0] == "Part");
    CHECK(result[1] == "TextLabel");
}

TEST_CASE("parse_selector_trailing_comma")
{
    auto result = parseClassNamesFromSelector("Part,");
    REQUIRE(result.size() == 1);
    CHECK(result[0] == "Part");
}

TEST_CASE("parse_selector_leading_comma")
{
    auto result = parseClassNamesFromSelector(",Part");
    REQUIRE(result.size() == 1);
    CHECK(result[0] == "Part");
}

TEST_CASE("parse_selector_bare_combinator")
{
    auto result = parseClassNamesFromSelector(">>");
    CHECK(result.empty());
}

TEST_CASE("parse_selector_special_chars_only")
{
    auto result = parseClassNamesFromSelector("...");
    CHECK(result.empty());
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("MagicFunctions");

TEST_CASE_FIXTURE(Fixture, "instance_new")
{
    auto result = check(R"(
        local x = Instance.new("Part")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("x")) == "Part");
}

TEST_CASE_FIXTURE(Fixture, "get_service")
{
    auto result = check(R"(
        local ReplicatedStorage = game:GetService("ReplicatedStorage")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("ReplicatedStorage")) == "ReplicatedStorage");
}

TEST_CASE_FIXTURE(Fixture, "get_service_unknown_service")
{
    auto result = check(R"(
        local ReplicatedStorage = game:GetService("Unknown")
    )");

    LUAU_LSP_REQUIRE_ERROR_COUNT(1, result);
    CHECK(toString(result.errors[0]) == "Invalid service name 'Unknown'");
}

TEST_CASE_FIXTURE(Fixture, "instance_is_a")
{
    auto result = check(R"(
        local x: Instance = Instance.new("Part")
        assert(x:IsA("TextLabel"))
        local y = x
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("y")) == "TextLabel");
}

TEST_CASE_FIXTURE(Fixture, "instance_is_a_unknown_class")
{
    auto result = check(R"(
        local x = Instance.new("Part")
        local y = x:IsA("unknown_class")
    )");

    LUAU_LSP_REQUIRE_ERROR_COUNT(1, result);
    CHECK(toString(result.errors[0]) == "Unknown type 'unknown_class'");
}

TEST_CASE_FIXTURE(Fixture, "instance_clone")
{
    auto result = check(R"(
        local x = Instance.new("Part")
        local y = x:Clone()
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("y")) == "Part");
}

TEST_CASE_FIXTURE(Fixture, "instance_from_existing")
{
    auto result = check(R"(
        local x = Instance.new("Part")
        local y = Instance.fromExisting(x)
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("y")) == "Part");
}

TEST_CASE_FIXTURE(Fixture, "find_first_x_which_is_a")
{
    auto result = check(R"(
        local x: Instance = Instance.new("Part")

        local a = x:FindFirstChildWhichIsA("TextLabel")
        local b = x:FindFirstChildOfClass("TextLabel")
        local c = x:FindFirstAncestorWhichIsA("TextLabel")
        local d = x:FindFirstAncestorOfClass("TextLabel")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("a")) == "TextLabel?");
    CHECK(Luau::toString(requireType("b")) == "TextLabel?");
    CHECK(Luau::toString(requireType("c")) == "TextLabel?");
    CHECK(Luau::toString(requireType("d")) == "TextLabel?");
}

TEST_CASE_FIXTURE(Fixture, "get_property_changed_signal")
{
    auto result = check(R"(
        local x = Instance.new("Part")
        local y = x:GetPropertyChangedSignal("Anchored")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
}

TEST_CASE_FIXTURE(Fixture, "get_property_changed_signal_unknown_property")
{
    auto result = check(R"(
        local x = Instance.new("Part")
        local y = x:GetPropertyChangedSignal("unknown")
    )");

    LUAU_LSP_REQUIRE_ERROR_COUNT(1, result);
    if (FFlag::LuauTypeCheckerUdtfRenameClassToExtern)
        CHECK(toString(result.errors[0]) == "Key 'unknown' not found in external type 'Part'");
    else
        CHECK(toString(result.errors[0]) == "Key 'unknown' not found in class 'Part'");
}

TEST_CASE_FIXTURE(Fixture, "enum_is_a")
{
    auto result = check(R"(
        local x: EnumItem = Enum.HumanoidRigType.R15
        assert(x:IsA("HumanoidRigType"))
        local y = x
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("y")) == "Enum.HumanoidRigType");
}

TEST_CASE_FIXTURE(Fixture, "enum_is_a_unknown_enum")
{
    auto result = check(R"(
        local x = Enum.HumanoidRigType.R15
        local y = x:IsA("unknown")
    )");

    LUAU_LSP_REQUIRE_ERROR_COUNT(1, result);
    CHECK(toString(result.errors[0]) == "Unknown type 'unknown'");
}

TEST_CASE_FIXTURE(Fixture, "typeof_refines_for_instance")
{
    auto result = check(R"(
        local obj: unknown = game
        assert(typeof(obj) == "Instance")
        local realObj = obj
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("realObj")) == "Instance");
}

TEST_CASE_FIXTURE(Fixture, "is_property_modified")
{
    auto result = check(R"(
        local x = Instance.new("Part")
        local y = x:IsPropertyModified("Anchored")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
}

TEST_CASE_FIXTURE(Fixture, "is_property_modified_unknown_property")
{
    auto result = check(R"(
        local x = Instance.new("Part")
        local y = x:IsPropertyModified("unknown")
    )");

    LUAU_LSP_REQUIRE_ERROR_COUNT(1, result);
    if (FFlag::LuauTypeCheckerUdtfRenameClassToExtern)
        CHECK(toString(result.errors[0]) == "Key 'unknown' not found in external type 'Part'");
    else
        CHECK(toString(result.errors[0]) == "Key 'unknown' not found in class 'Part'");
}

TEST_CASE_FIXTURE(Fixture, "reset_property_to_default")
{
    auto result = check(R"(
        local x = Instance.new("Part")
        local y = x:ResetPropertyToDefault("Anchored")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
}

TEST_CASE_FIXTURE(Fixture, "reset_property_to_default_unknown_property")
{
    auto result = check(R"(
        local x = Instance.new("Part")
        local y = x:ResetPropertyToDefault("unknown")
    )");

    LUAU_LSP_REQUIRE_ERROR_COUNT(1, result);
    if (FFlag::LuauTypeCheckerUdtfRenameClassToExtern)
        CHECK(toString(result.errors[0]) == "Key 'unknown' not found in external type 'Part'");
    else
        CHECK(toString(result.errors[0]) == "Key 'unknown' not found in class 'Part'");
}

TEST_CASE_FIXTURE(Fixture, "query_descendants_no_class")
{
    auto result = check(R"(
        local x: Instance = Instance.new("Part")
        local y = x:QueryDescendants(".Tag")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("y")) == "{Instance}");
}

TEST_CASE_FIXTURE(Fixture, "query_descendants_single_class")
{
    auto result = check(R"(
        local x: Instance = Instance.new("Part")
        local y = x:QueryDescendants("BasePart")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("y")) == "{BasePart}");
}

TEST_CASE_FIXTURE(Fixture, "query_descendants_multiple_classes")
{
    auto result = check(R"(
        local x: Instance = Instance.new("Part")
        local y = x:QueryDescendants("Part, TextLabel")
    )");

    LUAU_LSP_REQUIRE_NO_ERRORS(result);
    CHECK(Luau::toString(requireType("y")) == "{Part | TextLabel}");
}

TEST_CASE_FIXTURE(Fixture, "query_descendants_unknown_class")
{
    auto result = check(R"(
        local x: Instance = Instance.new("Part")
        local y = x:QueryDescendants("UnknownClass")
    )");

    LUAU_LSP_REQUIRE_ERROR_COUNT(1, result);
    CHECK(toString(result.errors[0]) == "Unknown type 'UnknownClass'");
}

TEST_SUITE_END();
