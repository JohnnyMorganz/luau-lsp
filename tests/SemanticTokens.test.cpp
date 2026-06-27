#include "doctest.h"
#include "Fixture.h"
#include "LSP/SemanticTokens.hpp"

TEST_SUITE_BEGIN("SemanticTokens");

std::optional<SemanticToken> getSemanticToken(const std::vector<SemanticToken>& tokens, const Luau::Position& start)
{
    for (const auto& token : tokens)
        if (token.start == start)
            return token;
    return std::nullopt;
}

TEST_CASE_FIXTURE(Fixture, "explicit_self_method_has_method_semantic_token")
{
    check(R"(
        type myClass = {
            foo: (self: myClass) -> ()
        }
        local a: myClass = nil
        a:foo()
    )");

    auto tokens = getSemanticTokens(workspace.frontend, getMainModule(), getMainSourceModule());
    REQUIRE(!tokens.empty());

    auto token = getSemanticToken(tokens, Luau::Position{5, 10});
    REQUIRE(token);
    CHECK_EQ(token->tokenType, lsp::SemanticTokenTypes::Method);
    CHECK_EQ(token->tokenModifiers, lsp::SemanticTokenModifiers::None);
}

TEST_CASE_FIXTURE(Fixture, "explicit_self_overloaded_method_has_method_semantic_token")
{
    check(R"(
        type myClass = {
            foo: ((self: myClass) -> ()) & ((self: myClass, myArg: boolean) -> ()),
        }
        local a: myClass = nil
        a:foo()
    )");

    auto tokens = getSemanticTokens(workspace.frontend, getMainModule(), getMainSourceModule());
    REQUIRE(!tokens.empty());

    auto token = getSemanticToken(tokens, Luau::Position{5, 10});
    REQUIRE(token);
    CHECK_EQ(token->tokenType, lsp::SemanticTokenTypes::Method);
    CHECK_EQ(token->tokenModifiers, lsp::SemanticTokenModifiers::None);
}

TEST_CASE_FIXTURE(Fixture, "references_of_self_within_a_method_have_semantic_tokens")
{
    check(R"(
        local T = {}
        function T:foo()
            self.value = true
        end
    )");

    auto tokens = getSemanticTokens(workspace.frontend, getMainModule(), getMainSourceModule());
    REQUIRE(!tokens.empty());

    auto token = getSemanticToken(tokens, Luau::Position{3, 12});
    REQUIRE(token);
    CHECK_EQ(token->tokenType, lsp::SemanticTokenTypes::Property);
    CHECK_EQ(token->tokenModifiers, lsp::SemanticTokenModifiers::DefaultLibrary);
}

TEST_CASE_FIXTURE(Fixture, "references_of_explicitly_declared_self_in_a_non_method_function_definition_have_semantic_token")
{
    check(R"(
        local T = {}
        function T.foo(self: any)
            self.value = true
        end
    )");

    auto tokens = getSemanticTokens(workspace.frontend, getMainModule(), getMainSourceModule());
    REQUIRE(!tokens.empty());

    auto parameter = getSemanticToken(tokens, Luau::Position{2, 23});
    REQUIRE(parameter);
    CHECK_EQ(parameter->tokenType, lsp::SemanticTokenTypes::Property);
    CHECK_EQ(parameter->tokenModifiers, lsp::SemanticTokenModifiers::DefaultLibrary);

    auto usage = getSemanticToken(tokens, Luau::Position{3, 12});
    REQUIRE(usage);
    CHECK_EQ(usage->tokenType, lsp::SemanticTokenTypes::Property);
    CHECK_EQ(usage->tokenModifiers, lsp::SemanticTokenModifiers::DefaultLibrary);
}

TEST_CASE_FIXTURE(Fixture, "explicitly_declared_self_parameter_does_not_have_semantic_token_modifier_in_method_function_definition")
{
    check(R"(
        local T = {}
        function T:foo(self: any)
        end
    )");

    auto tokens = getSemanticTokens(workspace.frontend, getMainModule(), getMainSourceModule());
    REQUIRE(!tokens.empty());

    auto parameter = getSemanticToken(tokens, Luau::Position{2, 23});
    REQUIRE(parameter);
    CHECK_EQ(parameter->tokenType, lsp::SemanticTokenTypes::Parameter);
    CHECK_EQ(parameter->tokenModifiers, lsp::SemanticTokenModifiers::None);
}

TEST_CASE_FIXTURE(Fixture,
    "references_of_explicitly_declared_self_in_a_non_method_function_definition_does_not_have_semantic_token_if_self_was_not_first_parameter")
{
    check(R"(
        local T = {}
        function T.foo(input: any, self: any)
            self.value = true
        end
    )");

    auto tokens = getSemanticTokens(workspace.frontend, getMainModule(), getMainSourceModule());
    REQUIRE(!tokens.empty());

    auto parameter = getSemanticToken(tokens, Luau::Position{2, 35});
    REQUIRE(parameter);
    CHECK_EQ(parameter->tokenType, lsp::SemanticTokenTypes::Parameter);
    CHECK_EQ(parameter->tokenModifiers, lsp::SemanticTokenModifiers::None);

    auto usage = getSemanticToken(tokens, Luau::Position{3, 12});
    REQUIRE(usage);
    CHECK_EQ(usage->tokenType, lsp::SemanticTokenTypes::Parameter);
    CHECK_EQ(usage->tokenModifiers, lsp::SemanticTokenModifiers::None);
}

TEST_CASE_FIXTURE(Fixture, "references_of_parameters_in_function_body_have_parameter_semantic_token")
{
    check(R"(
        local function foo(value: number)
            local x = value
        end
    )");

    auto tokens = getSemanticTokens(workspace.frontend, getMainModule(), getMainSourceModule());
    REQUIRE(!tokens.empty());

    // `        local function foo(value: number)` — `value` at col 27
    auto paramDecl = getSemanticToken(tokens, Luau::Position{1, 27});
    REQUIRE(paramDecl);
    CHECK_EQ(paramDecl->tokenType, lsp::SemanticTokenTypes::Parameter);
    CHECK_EQ(paramDecl->tokenModifiers, lsp::SemanticTokenModifiers::None);

    // `            local x = value` — `value` at col 22
    auto paramUsage = getSemanticToken(tokens, Luau::Position{2, 22});
    REQUIRE(paramUsage);
    CHECK_EQ(paramUsage->tokenType, lsp::SemanticTokenTypes::Parameter);
    CHECK_EQ(paramUsage->tokenModifiers, lsp::SemanticTokenModifiers::None);
}

TEST_CASE_FIXTURE(Fixture, "function_typed_parameter_references_in_body_are_not_reclassified_as_function_tokens")
{
    check(R"(
        local function foo(callback: () -> ())
            callback()
        end
    )");

    auto tokens = getSemanticTokens(workspace.frontend, getMainModule(), getMainSourceModule());
    REQUIRE(!tokens.empty());

    // `        local function foo(callback: () -> ())` — `callback` at col 27
    auto paramDecl = getSemanticToken(tokens, Luau::Position{1, 27});
    REQUIRE(paramDecl);
    CHECK_EQ(paramDecl->tokenType, lsp::SemanticTokenTypes::Parameter);
    CHECK_EQ(paramDecl->tokenModifiers, lsp::SemanticTokenModifiers::None);

    // `            callback()` — `callback` at col 12
    auto paramUsage = getSemanticToken(tokens, Luau::Position{2, 12});
    REQUIRE(paramUsage);
    CHECK_EQ(paramUsage->tokenType, lsp::SemanticTokenTypes::Parameter);
    CHECK_EQ(paramUsage->tokenModifiers, lsp::SemanticTokenModifiers::None);
}

TEST_CASE_FIXTURE(Fixture, "semantic_tokens_respects_cancellation")
{
    auto cancellationToken = std::make_shared<Luau::FrontendCancellationToken>();
    cancellationToken->cancel();
    
    auto document = newDocument("a.luau", "local x = 1");
    CHECK_THROWS_AS(workspace.semanticTokens(lsp::SemanticTokensParams{{{document}}}, cancellationToken), RequestCancelledException);
}

TEST_SUITE_END();
