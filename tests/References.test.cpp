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
            return a.uri.toString() <= b.uri.toString() && a.range.start < b.range.start;
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
    auto references = workspace.findAllTableReferences(ty, "name");
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
    auto references = workspace.findAllTableReferences(ty, "name");
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
    auto references = workspace.findAllTableReferences(ty, "name");
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
    auto references = workspace.findAllTableReferences(ty, "name");
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

TEST_CASE_FIXTURE(Fixture, "find_references_of_type_definition_used_as_return_type")
{
    auto source = R"(
        type Foo = {}

        local function foo(): Foo
        end
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::ReferenceParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 14}; // 'Foo' symbol

    auto result = workspace.references(params);
    REQUIRE(result);
    REQUIRE_EQ(2, result->size());

    sortResults(result);

    CHECK_EQ(lsp::Range{{1, 13}, {1, 16}}, result->at(0).range);
    CHECK_EQ(lsp::Range{{3, 30}, {3, 33}}, result->at(1).range);
}

TEST_CASE_FIXTURE(Fixture, "cross_module_find_references_of_a_returned_local_function")
{
    auto uri = newDocument("useFunction.luau", R"(
        local function useFunction()
        end

        return useFunction
    )");

    auto user = newDocument("user.luau", R"(
        local useFunction = require("useFunction.luau")

        local value = useFunction()
    )");

    // Index reverse deps
    workspace.frontend.parse(workspace.fileResolver.getModuleName(user));

    lsp::ReferenceParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 28}; // 'useFunction' definition

    auto result = workspace.references(params);
    REQUIRE(result);

    // The new solver does not store `require("useFunction.luau")` in the astTypes of a module
    // So we fail to resolve it as a reference. Unsure if this *should* be resolved.
    // Note that `require("useFunction.luau")()` *would* resolve as a reference.
    if (FFlag::LuauSolverV2)
        REQUIRE_EQ(3, result->size());
    else
        REQUIRE_EQ(4, result->size());

    sortResults(result);

    CHECK_EQ(result->at(0).uri, uri);
    CHECK_EQ(result->at(0).range, lsp::Range{{1, 23}, {1, 34}});
    CHECK_EQ(result->at(1).uri, uri);
    CHECK_EQ(result->at(1).range, lsp::Range{{4, 15}, {4, 26}});
    if (FFlag::LuauSolverV2)
    {
        CHECK_EQ(result->at(2).uri, user);
        CHECK_EQ(result->at(2).range, lsp::Range{{3, 22}, {3, 33}});
    }
    else
    {
        CHECK_EQ(result->at(2).uri, user);
        CHECK_EQ(result->at(2).range, lsp::Range{{1, 28}, {1, 55}});
        CHECK_EQ(result->at(3).uri, user);
        CHECK_EQ(result->at(3).range, lsp::Range{{3, 22}, {3, 33}});
    }
}

TEST_CASE_FIXTURE(Fixture, "cross_module_find_references_of_a_returned_global_function")
{
    auto uri = newDocument("useFunction.luau", R"(
        function useFunction()
        end

        return useFunction
    )");

    auto user = newDocument("user.luau", R"(
        local useFunction = require("useFunction.luau")

        local value = useFunction()
    )");

    // Index reverse deps
    workspace.frontend.parse(workspace.fileResolver.getModuleName(user));

    lsp::ReferenceParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 20}; // 'useFunction' definition

    auto result = workspace.references(params);
    REQUIRE(result);

    // The new solver does not store `require("useFunction.luau")` in the astTypes of a module
    // So we fail to resolve it as a reference. Unsure if this *should* be resolved.
    // Note that `require("useFunction.luau")()` *would* resolve as a reference.
    if (FFlag::LuauSolverV2)
        REQUIRE_EQ(3, result->size());
    else
        REQUIRE_EQ(4, result->size());

    sortResults(result);

    CHECK_EQ(result->at(0).uri, uri);
    CHECK_EQ(result->at(0).range, lsp::Range{{1, 17}, {1, 28}});
    CHECK_EQ(result->at(1).uri, uri);
    CHECK_EQ(result->at(1).range, lsp::Range{{4, 15}, {4, 26}});
    if (FFlag::LuauSolverV2)
    {
        CHECK_EQ(result->at(2).uri, user);
        CHECK_EQ(result->at(2).range, lsp::Range{{3, 22}, {3, 33}});
    }
    else
    {
        CHECK_EQ(result->at(2).uri, user);
        CHECK_EQ(result->at(2).range, lsp::Range{{1, 28}, {1, 55}});
        CHECK_EQ(result->at(3).uri, user);
        CHECK_EQ(result->at(3).range, lsp::Range{{3, 22}, {3, 33}});
    }
}

TEST_CASE_FIXTURE(Fixture, "cross_module_find_references_of_a_returned_table")
{
    auto uri = newDocument("tbl.luau", R"(
        local tbl = {}

        return tbl
    )");

    auto user = newDocument("user.luau", R"(
        local tbl = require("tbl.luau")

        local value = tbl
    )");

    // Index reverse deps
    workspace.frontend.parse(workspace.fileResolver.getModuleName(user));

    lsp::ReferenceParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 16}; // 'tbl' definition

    auto result = workspace.references(params);
    REQUIRE(result);

    // The new solver does not store `require("useFunction.luau")` in the astTypes of a module
    // So we fail to resolve it as a reference. Unsure if this *should* be resolved.
    // Note that `require("useFunction.luau")()` *would* resolve as a reference.
    if (FFlag::LuauSolverV2)
        REQUIRE_EQ(3, result->size());
    else
        REQUIRE_EQ(4, result->size());

    sortResults(result);

    CHECK_EQ(result->at(0).uri, uri);
    CHECK_EQ(result->at(0).range, lsp::Range{{1, 20}, {1, 22}});
    CHECK_EQ(result->at(1).uri, uri);
    CHECK_EQ(result->at(1).range, lsp::Range{{3, 15}, {3, 18}});
    if (FFlag::LuauSolverV2)
    {
        CHECK_EQ(result->at(2).uri, user);
        CHECK_EQ(result->at(2).range, lsp::Range{{3, 22}, {3, 25}});
    }
    else
    {
        CHECK_EQ(result->at(2).uri, user);
        CHECK_EQ(result->at(2).range, lsp::Range{{1, 20}, {1, 39}});
        CHECK_EQ(result->at(3).uri, user);
        CHECK_EQ(result->at(3).range, lsp::Range{{3, 22}, {3, 25}});
    }
}


TEST_SUITE_END();
