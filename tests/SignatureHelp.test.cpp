#include "doctest.h"
#include "Fixture.h"
#include "Platform/RobloxPlatform.hpp"

TEST_SUITE_BEGIN("SignatureHelp");

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

    auto result = workspace.signatureHelp(params);
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

TEST_SUITE_END();
