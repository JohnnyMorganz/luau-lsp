#include "doctest.h"
#include "Fixture.h"

#include "LSP/DocumentationParser.hpp"

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

TEST_CASE_FIXTURE(Fixture, "print_comments")
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

    auto comments = getComments(ftv->definition->definitionLocation);
    REQUIRE_EQ(1, comments.size());
    CHECK(comments[0] == "Adds 5 to the input number");
}

TEST_CASE_FIXTURE(Fixture, "print_comments_2")
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

    auto comments = getComments(ftv->definition->definitionLocation);
    REQUIRE_EQ(1, comments.size());
    CHECK(comments[0] == "Adds 5 to the input number");
}

TEST_CASE_FIXTURE(Fixture, "print_multiline_comment")
{
    auto result = check(R"(
        --[=[
            Adds 5 to the input number

            @param x number -- The number to add 5 to
            @return number -- Returns `x` with 5 added to it
        ]=]
        function add5(x: number)
            return x + 5
        end
    )");

    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("add5");
    auto ftv = Luau::get<Luau::FunctionTypeVar>(ty);
    REQUIRE(ftv);
    REQUIRE(ftv->definition);

    auto comments = getComments(ftv->definition->definitionLocation);
    REQUIRE_EQ(4, comments.size());
    CHECK(comments[0] == "Adds 5 to the input number");
    CHECK(comments[1] == "");
    CHECK(comments[2] == "@param x number -- The number to add 5 to");
    CHECK(comments[3] == "@return number -- Returns `x` with 5 added to it");
}

TEST_CASE_FIXTURE(Fixture, "trim_common_leading_whitespace")
{
    auto result = check(R"(
        --[=[
            Adds 5 to the input number

            ```lua
            do
                local x = 5
            end
            ```
        ]=]
        function foo(x: number)
            return x + 5
        end
    )");

    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("foo");
    auto ftv = Luau::get<Luau::FunctionTypeVar>(ty);
    REQUIRE(ftv);
    REQUIRE(ftv->definition);

    auto comments = getComments(ftv->definition->definitionLocation);
    REQUIRE_EQ(7, comments.size());
    CHECK(comments[0] == "Adds 5 to the input number");
    CHECK(comments[1] == "");
    CHECK(comments[2] == "```lua");
    CHECK(comments[3] == "do");
    CHECK(comments[4] == "    local x = 5");
    CHECK(comments[5] == "end");
    CHECK(comments[6] == "```");
}


TEST_CASE_FIXTURE(Fixture, "nested_functions")
{
    auto result = check(R"(
        --- Foo function
        function foo()
            --- Bar function
            function bar()
            end
        end
    )");

    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("bar");
    auto ftv = Luau::get<Luau::FunctionTypeVar>(ty);
    REQUIRE(ftv);
    REQUIRE(ftv->definition);

    auto comments = getComments(ftv->definition->definitionLocation);
    REQUIRE_EQ(1, comments.size());
    CHECK(comments[0] == "Bar function");
}

TEST_CASE_FIXTURE(Fixture, "print_moonwave_documentation")
{
    auto result = check(R"(
        --[=[
            Adds 5 to the input number
            @param x number -- Input number
            @return number
        ]=]
        function foo(x: number)
            return x + 5
        end
    )");

    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("foo");
    auto ftv = Luau::get<Luau::FunctionTypeVar>(ty);
    REQUIRE(ftv);
    REQUIRE(ftv->definition);

    auto comments = getComments(ftv->definition->definitionLocation);
    auto documentation = printMoonwaveDocumentation(comments);

    CHECK_EQ(documentation, "Adds 5 to the input number\n"
                            "\n\n**Parameters**"
                            "\n- `x` number -- Input number"
                            "\n\n**Returns**"
                            "\n- number");
}

TEST_CASE_FIXTURE(Fixture, "print_throws_info")
{
    auto result = check(R"(
        --[=[
            Adds 5 to the input number
            @param x number -- Input number
            @return number
            @error NotANumber -- Input is not a number
        ]=]
        function foo(x: number)
            return x + 5
        end
    )");

    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("foo");
    auto ftv = Luau::get<Luau::FunctionTypeVar>(ty);
    REQUIRE(ftv);
    REQUIRE(ftv->definition);

    auto comments = getComments(ftv->definition->definitionLocation);
    auto documentation = printMoonwaveDocumentation(comments);

    CHECK_EQ(documentation, "Adds 5 to the input number\n"
                            "\n\n**Parameters**"
                            "\n- `x` number -- Input number"
                            "\n\n**Returns**"
                            "\n- number"
                            "\n\n**Throws**"
                            "\n- `NotANumber` -- Input is not a number");
}

TEST_CASE_FIXTURE(Fixture, "ignored_tags")
{
    auto result = check(R"(
        --[=[
            Adds 5 to the input number
            @param x number -- Testing
            @tag Testing
            @within X.Y.Z
        ]=]
        function foo(x: number)
            return x + 5
        end
    )");

    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("foo");
    auto ftv = Luau::get<Luau::FunctionTypeVar>(ty);
    REQUIRE(ftv);
    REQUIRE(ftv->definition);

    auto comments = getComments(ftv->definition->definitionLocation);
    auto documentation = printMoonwaveDocumentation(comments);

    CHECK_EQ(documentation, "Adds 5 to the input number\n"
                            "\n\n**Parameters**"
                            "\n- `x` number -- Testing");
}

TEST_SUITE_END();