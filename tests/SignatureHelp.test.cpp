#include "doctest.h"
#include "Fixture.h"
#include "Platform/RobloxPlatform.hpp"
#include "ScopedFlags.h"

LUAU_FASTFLAG(LuauSolverV2)

TEST_SUITE_BEGIN("SignatureHelp");

TEST_CASE_FIXTURE(Fixture, "signature_help_handles_overloaded_functions_and_picks_best_overload_1")
{
    auto [source, marker] = sourceWithMarker(R"(
        type Foo = ((foo: string, bar: number) -> number) & ((bar: number, foo: string) -> string)
        local x: Foo

        x(1|)
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 2);

    CHECK_EQ(result->signatures[0].label, "function x(foo: string, bar: number): number");
    CHECK_EQ(result->signatures[1].label, "function x(bar: number, foo: string): string");
    CHECK_EQ(result->activeSignature, 1);
    CHECK_EQ(result->activeParameter, 0);
}

TEST_CASE_FIXTURE(Fixture, "signature_help_handles_overloaded_functions_and_picks_best_overload_1")
{
    auto [source, marker] = sourceWithMarker(R"(
        type Foo = ((foo: string, bar: number) -> number) & ((bar: number) -> string)
        local x: Foo

        x(1|)
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 2);

    CHECK_EQ(result->signatures[0].label, "function x(foo: string, bar: number): number");
    CHECK_EQ(result->signatures[1].label, "function x(bar: number): string");
    CHECK_EQ(result->activeSignature, 1);
    CHECK_EQ(result->activeParameter, 0);
}

TEST_CASE_FIXTURE(Fixture, "signature_help_shows_for_call_metamethod")
{
    auto [source, marker] = sourceWithMarker(R"(
        local mt = {}

        --- some documentation
        function mt.__call(self: any, meow: string)
        end

        local tbl = setmetatable({}, mt)

        tbl(|)
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 1);

    CHECK_EQ(result->signatures[0].label, "function tbl(self: any, meow: string): ()");
    REQUIRE(result->signatures[0].documentation);
    CHECK_EQ(result->signatures[0].documentation->value, "some documentation\n");
    REQUIRE(result->signatures[0].parameters);
    REQUIRE_EQ(result->signatures[0].parameters->size(), 2);

    CHECK_EQ(std::get<std::vector<size_t>>(result->signatures[0].parameters->at(0).label), std::vector<size_t>{13, 22});
    CHECK_EQ(std::get<std::vector<size_t>>(result->signatures[0].parameters->at(1).label), std::vector<size_t>{24, 36});
}

TEST_CASE_FIXTURE(Fixture, "signature_help_respects_cancellation")
{
    auto cancellationToken = std::make_shared<Luau::FrontendCancellationToken>();
    cancellationToken->cancel();

    auto document = newDocument("a.luau", "local x = 1");
    CHECK_THROWS_AS(workspace.signatureHelp(lsp::SignatureHelpParams{{{document}}}, cancellationToken), RequestCancelledException);
}

TEST_CASE_FIXTURE(Fixture, "signature_help_does_not_show_first_argument_on_method_calls_from_type_function_ftv")
{
    ScopedFastFlag sff{FFlag::LuauSolverV2, true};

    auto [source, marker] = sourceWithMarker(R"(
        type function repro()
            return types.newfunction({ head = { types.any, types.number } }, { head = { types.boolean } })
        end

        local foo: { bar: repro<> } = {
            bar = function(self: any, n: number): boolean
                return true
            end
        }

        foo:bar(|)
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 1);

    // TODO: would be nice if this was just `bar(number): boolean` https://github.com/JohnnyMorganz/luau-lsp/issues/1250#issuecomment-3592436704
    CHECK_EQ(result->signatures[0].label, "function foo:bar(any, number): boolean");
    REQUIRE(result->signatures[0].parameters);
    REQUIRE_EQ(result->signatures[0].parameters->size(), 1);

    CHECK_EQ(std::get<std::vector<size_t>>(result->signatures[0].parameters->at(0).label), std::vector<size_t>{22, 28});
}

TEST_SUITE_END();
