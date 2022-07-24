#include "doctest.h"
#include "Fixture.h"
#include "LSP/SemanticTokens.hpp"

TEST_SUITE_BEGIN("SemanticTokens");

TEST_CASE_FIXTURE(Fixture, "function_definition_name")
{
    auto result = parse(R"(
        function foo() end
    )");

    auto tokens = getSemanticTokens(result);
    REQUIRE(!tokens.empty());

    auto token = *tokens.begin();
    CHECK_EQ(token.tokenType, lsp::SemanticTokenTypes::Function);
    CHECK_EQ(token.tokenModifiers, lsp::SemanticTokenModifiers::None);
}

TEST_SUITE_END();