#include "doctest.h"
#include "Analyze/CliConfigurationParser.hpp"

TEST_SUITE_BEGIN("CliConfigurationParser");

TEST_CASE("convert dotted dictionary")
{
    auto output = parseDottedConfiguration(R"DOTTED_CONFIG(
        {
            "luau-lsp.foo.bar": true,
            "luau-lsp.foo.baz": 1,
            "luau-lsp.bar": "testing"
        }
    )DOTTED_CONFIG");

    REQUIRE(output.is_object());
    REQUIRE(output.contains("foo"));
    REQUIRE(output.contains("bar"));
    REQUIRE(output["foo"].is_object());

    CHECK_EQ(output["foo"]["bar"], true);
    CHECK_EQ(output["foo"]["baz"], 1);
    CHECK_EQ(output["bar"], "testing");
}

TEST_SUITE_END();