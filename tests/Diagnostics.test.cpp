#include "doctest.h"
#include "Fixture.h"

TEST_SUITE_BEGIN("Diagnostics");

TEST_CASE_FIXTURE(Fixture, "instance_is_a")
{
    auto result = check(R"(
        local x: Instance = Instance.new("Part")
        assert(x:IsA("TextLabel"))
        local y = x
    )");

    REQUIRE_EQ(0, result.errors.size());
    CHECK(Luau::toString(requireType("y")) == "TextLabel");
}

TEST_CASE_FIXTURE(Fixture, "instance_is_a_unknown_class")
{
    auto result = check(R"(
        local x = Instance.new("Part")
        local y = x:IsA("unknown")
    )");

    REQUIRE_EQ(1, result.errors.size());
    CHECK(toString(result.errors[0]) == "Unknown type 'unknown'");
}

TEST_CASE_FIXTURE(Fixture, "get_property_changed_signal")
{
    auto result = check(R"(
        local x = Instance.new("Part")
        local y = x:GetPropertyChangedSignal("Anchored")
    )");

    REQUIRE_EQ(0, result.errors.size());
}

TEST_CASE_FIXTURE(Fixture, "get_property_changed_signal_unknown_property")
{
    auto result = check(R"(
        local x = Instance.new("Part")
        local y = x:GetPropertyChangedSignal("unknown")
    )");

    REQUIRE_EQ(1, result.errors.size());
    CHECK(toString(result.errors[0]) == "Key 'unknown' not found in class 'Part'");
}

TEST_CASE_FIXTURE(Fixture, "enum_is_a")
{
    auto result = check(R"(
        local x: EnumItem = Enum.HumanoidRigType.R15
        assert(x:IsA("HumanoidRigType"))
        local y = x
    )");

    REQUIRE_EQ(0, result.errors.size());
    CHECK(Luau::toString(requireType("y")) == "Enum.HumanoidRigType");
}

TEST_CASE_FIXTURE(Fixture, "enum_is_a_unknown_enum")
{
    auto result = check(R"(
        local x = Enum.HumanoidRigType.R15
        local y = x:IsA("unknown")
    )");

    REQUIRE_EQ(1, result.errors.size());
    CHECK(toString(result.errors[0]) == "Unknown type 'unknown'");
}

TEST_SUITE_END();