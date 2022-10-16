#include "doctest.h"
#include "Fixture.h"

TEST_SUITE_BEGIN("Documentation");

TEST_CASE_FIXTURE(Fixture, "attach_comments_to_function")
{
    auto result = check(R"(
        --- Adds 5 to the input number
        function add5(x: number)
            return x + 5
        end
    )");

    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("add5");
    auto ftv = Luau::get<Luau::FunctionTypeVar>(ty);
    REQUIRE(ftv);
    REQUIRE(ftv->definition);

    auto comments = getCommentLocations(getMainSourceModule(), ftv->definition->definitionLocation);
    CHECK_EQ(1, comments.size());
}

TEST_CASE_FIXTURE(Fixture, "attach_comments_to_function_2")
{
    auto result = check(R"(
        --- Another doc comment
        function foo()
        end

        --- Adds 5 to the input number
        function add5(x: number)
            return x + 5
        end
    )");

    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("add5");
    auto ftv = Luau::get<Luau::FunctionTypeVar>(ty);
    REQUIRE(ftv);
    REQUIRE(ftv->definition);

    auto comments = getCommentLocations(getMainSourceModule(), ftv->definition->definitionLocation);
    CHECK_EQ(1, comments.size());
}

TEST_SUITE_END();