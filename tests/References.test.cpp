#include "doctest.h"
#include "Fixture.h"

#include "LSP/IostreamHelpers.hpp"

TEST_SUITE_BEGIN("References");

// TODO: cross module tests
// TODO: type references tests (cross module)

static void sortResults(std::optional<std::vector<lsp::Location>>& result)
{
    std::sort(result->begin(), result->end(),
        [](const lsp::Location& a, const lsp::Location& b)
        {
            return a.uri.toString() < b.uri.toString() || a.range.start < b.range.start;
        });
}

TEST_CASE_FIXTURE(Fixture, "find_table_property_declaration_1")
{
    auto result = check(R"(
        local T = {}
        T.name = "testing"
    )");
    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("T");
    auto references = workspace.findAllReferences(ty, "name");
    REQUIRE_EQ(1, references.size());
    CHECK(references[0].location.begin.line == 2);
    CHECK(references[0].location.begin.column == 10);
    CHECK(references[0].location.end.line == 2);
    CHECK(references[0].location.end.column == 14);
}

TEST_CASE_FIXTURE(Fixture, "find_table_property_declaration_2")
{
    auto result = check(R"(
        local T = {
            name = "Testing"
        }
    )");
    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("T");
    auto references = workspace.findAllReferences(ty, "name");
    REQUIRE_EQ(1, references.size());
    CHECK(references[0].location.begin.line == 2);
    CHECK(references[0].location.begin.column == 12);
    CHECK(references[0].location.end.line == 2);
    CHECK(references[0].location.end.column == 16);
}

TEST_CASE_FIXTURE(Fixture, "find_table_property_declaration_3")
{
    auto result = check(R"(
        local T = {}

        function T.name()
            return "testing"
        end
    )");
    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("T");
    auto references = workspace.findAllReferences(ty, "name");
    REQUIRE_EQ(1, references.size());
    CHECK(references[0].location.begin.line == 3);
    CHECK(references[0].location.begin.column == 19);
    CHECK(references[0].location.end.line == 3);
    CHECK(references[0].location.end.column == 23);
}

TEST_CASE_FIXTURE(Fixture, "find_table_property_declaration_4")
{
    auto result = check(R"(
        local T = {}

        function T:name()
            return "testing"
        end
    )");
    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("T");
    auto references = workspace.findAllReferences(ty, "name");
    REQUIRE_EQ(1, references.size());
    CHECK(references[0].location.begin.line == 3);
    CHECK(references[0].location.begin.column == 19);
    CHECK(references[0].location.end.line == 3);
    CHECK(references[0].location.end.column == 23);
}

TEST_CASE_FIXTURE(Fixture, "find_references_from_an_inline_table_property")
{
    // Finding reference of "name" inlined in "T"
    auto source = R"(
        local T = {
            name = "string"
        }

        local x = T.name
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::ReferenceParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{2, 12};

    auto result = workspace.references(params);
    REQUIRE(result);
    REQUIRE_EQ(2, result->size());

    sortResults(result);

    CHECK_EQ(lsp::Range{{2, 12}, {2, 16}}, result->at(0).range);
    CHECK_EQ(lsp::Range{{5, 20}, {5, 24}}, result->at(1).range);
}

TEST_CASE_FIXTURE(Fixture, "find_references_of_a_global_function")
{
    auto source = R"(
        function Main()
        end

        Main()
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::ReferenceParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{4, 11};

    auto result = workspace.references(params);
    REQUIRE(result);
    REQUIRE_EQ(2, result->size());

    sortResults(result);

    CHECK_EQ(lsp::Range{{1, 17}, {1, 21}}, result->at(0).range);
    CHECK_EQ(lsp::Range{{4, 8}, {4, 12}}, result->at(1).range);
}

TEST_CASE_FIXTURE(Fixture, "find_references_of_a_global_from_definitions_file")
{
    auto source = R"(
        local x = game:GetService("Foo")
        local y = game:GetService("Bar")
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::ReferenceParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 19}; // 'game' symbol

    auto result = workspace.references(params);
    REQUIRE(result);
    REQUIRE_EQ(2, result->size());

    sortResults(result);

    CHECK_EQ(lsp::Range{{1, 18}, {1, 22}}, result->at(0).range);
    CHECK_EQ(lsp::Range{{2, 18}, {2, 22}}, result->at(1).range);
}


TEST_SUITE_END();
