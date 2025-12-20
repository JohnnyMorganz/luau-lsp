#include "doctest.h"
#include "Fixture.h"

#include "Luau/AstQuery.h"
#include "LSP/DocumentationParser.hpp"
#include "LSP/LuauExt.hpp"

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
    auto ftv = Luau::get<Luau::FunctionType>(ty);
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
    auto ftv = Luau::get<Luau::FunctionType>(ty);
    REQUIRE(ftv);
    REQUIRE(ftv->definition);

    auto comments = getCommentLocations(getMainSourceModule(), ftv->definition->definitionLocation);
    CHECK_EQ(1, comments.size());
}

TEST_CASE_FIXTURE(Fixture, "attach_comments_to_table_property_function_1")
{
    auto result = check(R"(
        local tbl = {
            --- This is a function inside of a table
            values = function()
            end,
        }

        local x = tbl.values
    )");

    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("x");
    auto ftv = Luau::get<Luau::FunctionType>(ty);
    REQUIRE(ftv);
    REQUIRE(ftv->definition);

    auto comments = getCommentLocations(getMainSourceModule(), ftv->definition->definitionLocation);
    CHECK_EQ(1, comments.size());
}

TEST_CASE_FIXTURE(Fixture, "attach_comments_to_table_property_function_2")
{
    auto result = check(R"(
        local tbl = {
            --- This is a function inside of a table
            values = function()
            end,
            --- This is another function, and there should only be one comment connected to it
            map = function()
            end,
        }

        local x = tbl.map
    )");

    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("x");
    auto ftv = Luau::get<Luau::FunctionType>(ty);
    REQUIRE(ftv);
    REQUIRE(ftv->definition);

    auto comments = getCommentLocations(getMainSourceModule(), ftv->definition->definitionLocation);
    CHECK_EQ(1, comments.size());
}

TEST_CASE_FIXTURE(Fixture, "attach_comments_to_props_1")
{
    auto result = check(R"(
        local tbl = {
            --- This is some special information
            data = "hello",
        }

        local x = tbl.data
    )");

    REQUIRE_EQ(0, result.errors.size());

    auto expr = Luau::findExprOrLocalAtPosition(*getMainSourceModule(), Luau::Position{6, 24}).getExpr(); // Hovering on "tbl.data"
    REQUIRE(expr);

    auto index = expr->as<Luau::AstExprIndexName>();
    REQUIRE(index);

    auto parentTyPtr = getMainModule()->astTypes.find(index->expr);
    REQUIRE(parentTyPtr);
    auto parentTy = Luau::follow(*parentTyPtr);

    auto indexName = index->index.value;
    auto prop = lookupProp(parentTy, indexName);
    REQUIRE(prop);
    REQUIRE(prop->second.location);

    auto comments = getCommentLocations(getMainSourceModule(), prop->second.location.value());
    CHECK_EQ(1, comments.size());
}

TEST_CASE_FIXTURE(Fixture, "attach_comments_to_props_2")
{
    auto result = check(R"(
        --- doc comment
        local tbl = {
            --- This is some special information
            data = "hello",
        }

    )");

    REQUIRE_EQ(0, result.errors.size());

    // Assume hovering over a var "tbl.data", which would give the position set to the property
    auto comments = getCommentLocations(getMainSourceModule(), Luau::Location{{4, 19}, {4, 26}});
    CHECK_EQ(1, comments.size());
}

TEST_CASE_FIXTURE(Fixture, "attach_comments_to_variable_1")
{
    auto result = check(R"(
        --- doc comment
        local var = "yo"
    )");

    REQUIRE_EQ(0, result.errors.size());

    auto comments = getCommentLocations(getMainSourceModule(), Luau::Location{{2, 14}, {2, 17}});
    CHECK_EQ(1, comments.size());
}

TEST_CASE_FIXTURE(Fixture, "attach_comments_to_variable_2")
{
    auto result = check(R"(
        --- Another doc comment
        local foo = function()
        end
    )");

    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("foo");
    auto ftv = Luau::get<Luau::FunctionType>(ty);
    REQUIRE(ftv);
    REQUIRE(ftv->definition);

    auto comments = getCommentLocations(getMainSourceModule(), ftv->definition->definitionLocation);
    CHECK_EQ(1, comments.size());
}

TEST_CASE_FIXTURE(Fixture, "attach_comments_to_table_type_props")
{
    auto result = check(R"(
        type Foo = {
            --- A documentation comment
            map: () -> (),
        }
    )");

    REQUIRE_EQ(0, result.errors.size());

    // Assume hovering over a var "tbl.data", which would give the position set to the property
    auto comments = getCommentLocations(getMainSourceModule(), Luau::Location{{3, 17}, {3, 25}});
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
    auto ftv = Luau::get<Luau::FunctionType>(ty);
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
    auto ftv = Luau::get<Luau::FunctionType>(ty);
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
    auto ftv = Luau::get<Luau::FunctionType>(ty);
    REQUIRE(ftv);
    REQUIRE(ftv->definition);

    auto comments = getComments(ftv->definition->definitionLocation);
    REQUIRE_EQ(4, comments.size());
    CHECK(comments[0] == "Adds 5 to the input number");
    CHECK(comments[1] == "");
    CHECK(comments[2] == "@param x number -- The number to add 5 to");
    CHECK(comments[3] == "@return number -- Returns `x` with 5 added to it");
}

TEST_CASE_FIXTURE(Fixture, "print_multiline_comment_no_equals")
{
    auto result = check(R"(
        --[[
            Adds 5 to the input number

            @param x number -- The number to add 5 to
            @return number -- Returns `x` with 5 added to it
        ]]
        function add5(x: number)
            return x + 5
        end
    )");

    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("add5");
    auto ftv = Luau::get<Luau::FunctionType>(ty);
    REQUIRE(ftv);
    REQUIRE(ftv->definition);

    auto comments = getComments(ftv->definition->definitionLocation);
    REQUIRE_EQ(4, comments.size());
    CHECK(comments[0] == "Adds 5 to the input number");
    CHECK(comments[1] == "");
    CHECK(comments[2] == "@param x number -- The number to add 5 to");
    CHECK(comments[3] == "@return number -- Returns `x` with 5 added to it");
}


TEST_CASE_FIXTURE(Fixture, "print_multiline_comment_variable_equals")
{
    auto result = check(R"(
        --[====[
            Adds 5 to the input number

            @param x number -- The number to add 5 to
            @return number -- Returns `x` with 5 added to it
        ]====]
        function add5(x: number)
            return x + 5
        end
    )");

    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("add5");
    auto ftv = Luau::get<Luau::FunctionType>(ty);
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
    auto ftv = Luau::get<Luau::FunctionType>(ty);
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
    auto ftv = Luau::get<Luau::FunctionType>(ty);
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
    auto ftv = Luau::get<Luau::FunctionType>(ty);
    REQUIRE(ftv);
    REQUIRE(ftv->definition);

    auto comments = getComments(ftv->definition->definitionLocation);
    auto documentation = printMoonwaveDocumentation(comments);

    CHECK_EQ(documentation, "Adds 5 to the input number\n"
                            "\n\n**Parameters**\n"
                            "\n- `x` number -- Input number"
                            "\n\n**Returns**\n"
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
    auto ftv = Luau::get<Luau::FunctionType>(ty);
    REQUIRE(ftv);
    REQUIRE(ftv->definition);

    auto comments = getComments(ftv->definition->definitionLocation);
    auto documentation = printMoonwaveDocumentation(comments);

    CHECK_EQ(documentation, "Adds 5 to the input number\n"
                            "\n\n**Parameters**\n"
                            "\n- `x` number -- Input number"
                            "\n\n**Returns**\n"
                            "\n- number"
                            "\n\n**Throws**\n"
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
    auto ftv = Luau::get<Luau::FunctionType>(ty);
    REQUIRE(ftv);
    REQUIRE(ftv->definition);

    auto comments = getComments(ftv->definition->definitionLocation);
    auto documentation = printMoonwaveDocumentation(comments);

    CHECK_EQ(documentation, "Adds 5 to the input number\n"
                            "\n\n**Parameters**\n"
                            "\n- `x` number -- Testing");
}

TEST_CASE_FIXTURE(Fixture, "singleline_comments_preserve_newlines")
{
    auto result = check(R"(
        --- @class MyClass
        ---
        --- A sample class.
        local MyClass = {}
    )");

    REQUIRE_EQ(0, result.errors.size());

    auto ty = requireType("MyClass");
    auto ttv = Luau::get<Luau::TableType>(ty);
    REQUIRE(ttv);

    auto comments = getComments(ttv->definitionLocation);
    REQUIRE_EQ(3, comments.size());

    CHECK_EQ("@class MyClass", comments[0]);
    CHECK_EQ("\n", comments[1]);
    CHECK_EQ("A sample class.", comments[2]);
}

TEST_SUITE_END();
