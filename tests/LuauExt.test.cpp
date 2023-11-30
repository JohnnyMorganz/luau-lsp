#include "doctest.h"
#include "Fixture.h"
#include "LSP/LuauExt.hpp"
#include "Platform/RobloxPlatform.hpp"

TEST_SUITE_BEGIN("Luau Extensions");

TEST_CASE_FIXTURE(Fixture, "FindImports service location 1")
{
    auto block = parse(R"(
        local ReplicatedStorage = game:GetService("ReplicatedStorage")
    )");
    REQUIRE(block);

    RobloxFindImportsVisitor visitor;
    visitor.visit(block);

    CHECK_EQ(visitor.findBestLineForService("CollectionService", 0), 1);
    CHECK_EQ(visitor.findBestLineForService("ServerStorage", 0), 2);
}

TEST_CASE_FIXTURE(Fixture, "FindImports service location 2")
{
    auto block = parse(R"(
        local ReplicatedStorage = game:GetService("ReplicatedStorage")
        local ServerStorage = game:GetService("ServerStorage")
    )");
    REQUIRE(block);

    RobloxFindImportsVisitor visitor;
    visitor.visit(block);

    CHECK_EQ(visitor.findBestLineForService("CollectionService", 0), 1);
    CHECK_EQ(visitor.findBestLineForService("ServerScriptService", 0), 2);
    CHECK_EQ(visitor.findBestLineForService("Workspace", 0), 3);
}

TEST_CASE_FIXTURE(Fixture, "FindImports service location 3")
{
    auto block = parse(R"(
        local ReplicatedStorage =
            game:GetService("ReplicatedStorage")
    )");
    REQUIRE(block);

    RobloxFindImportsVisitor visitor;
    visitor.visit(block);

    CHECK_EQ(visitor.findBestLineForService("Workspace", 0), 3);
}

TEST_CASE_FIXTURE(Fixture, "FindImports require simple")
{
    auto block = parse(R"(
        local test = require(path.to.test)
    )");
    REQUIRE(block);

    FindImportsVisitor visitor;
    visitor.visit(block);

    CHECK(visitor.containsRequire("test"));
    CHECK_FALSE(visitor.containsRequire("foo"));

    REQUIRE(visitor.requiresMap.size() == 1);
    CHECK_EQ(visitor.requiresMap[0].begin()->second->location.end.line, 1);
}

TEST_CASE_FIXTURE(Fixture, "FindImports require multiline")
{
    auto block = parse(R"(
        local test =
            require(path.to.test)
    )");
    REQUIRE(block);

    FindImportsVisitor visitor;
    visitor.visit(block);

    CHECK(visitor.containsRequire("test"));
    CHECK_FALSE(visitor.containsRequire("foo"));

    REQUIRE(visitor.requiresMap.size() == 1);
    CHECK_EQ(visitor.requiresMap[0].begin()->second->location.end.line, 2);
}

TEST_SUITE_END();
