#include "doctest.h"
#include "Fixture.h"
#include "LSP/SemanticTokens.hpp"

TEST_SUITE_BEGIN("SemanticTokens");

TEST_CASE_FIXTURE(Fixture, "test")
{
    auto result = parse(R"(
        local x = 1
    )");

    auto tokens = getSemanticTokens(result);
    REQUIRE(!tokens.empty());
}

TEST_SUITE_END();