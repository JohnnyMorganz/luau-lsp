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

TEST_SUITE_END();