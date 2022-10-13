#include "doctest.h"
#include "Fixture.h"

TEST_SUITE_BEGIN("Diagnostics");

TEST_CASE_FIXTURE(Fixture, "get_property_changed_signal")
{
    auto result = check(R"(
        local x = Instance.new("Part")
        local y = x:GetPropertyChangedSignal("error")
    )");

    REQUIRE_EQ(1, result.errors.size());
    CHECK(toString(result.errors[0]) ==
          "Generic type 'A' is used as a variadic type parameter; consider changing 'A' to 'A...' in the generic argument list");
}

TEST_SUITE_END();