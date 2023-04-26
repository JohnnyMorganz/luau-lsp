#include "doctest.h"
#include "Fixture.h"

TEST_SUITE_BEGIN("References");

// TODO: cross module tests
// TODO: type references tests (cross module)

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


TEST_SUITE_END();