#include "doctest.h"
#include "Fixture.h"

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

TEST_SUITE_END();
