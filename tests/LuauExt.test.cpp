#include "doctest.h"
#include "Fixture.h"
#include "LSP/LuauExt.hpp"
#include "Platform/InstanceRequireAutoImporter.hpp"
#include "Platform/RobloxPlatform.hpp"

TEST_SUITE_BEGIN("Luau Extensions");

TEST_CASE_FIXTURE(Fixture, "FindImports service location 1")
{
    auto block = parse(R"(
        local ReplicatedStorage = game:GetService("ReplicatedStorage")
    )");
    REQUIRE(block);

    Luau::LanguageServer::AutoImports::RobloxFindImportsVisitor visitor;
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

    Luau::LanguageServer::AutoImports::RobloxFindImportsVisitor visitor;
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

    Luau::LanguageServer::AutoImports::RobloxFindImportsVisitor visitor;
    visitor.visit(block);

    CHECK_EQ(visitor.findBestLineForService("Workspace", 0), 3);
}

TEST_CASE_FIXTURE(Fixture, "FindImports require simple")
{
    auto block = parse(R"(
        local test = require(path.to.test)
    )");
    REQUIRE(block);

    Luau::LanguageServer::AutoImports::FindImportsVisitor visitor;
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

    Luau::LanguageServer::AutoImports::FindImportsVisitor visitor;
    visitor.visit(block);

    CHECK(visitor.containsRequire("test"));
    CHECK_FALSE(visitor.containsRequire("foo"));

    REQUIRE(visitor.requiresMap.size() == 1);
    CHECK_EQ(visitor.requiresMap[0].begin()->second->location.end.line, 2);
}

TEST_CASE_FIXTURE(Fixture, "lookupProp on a self-referential intersection type does not stack overflow")
{
    Luau::TypeArena arena;
    auto intersectionTypeId = arena.addType(Luau::IntersectionType{});
    auto itv = Luau::getMutable<Luau::IntersectionType>(intersectionTypeId);
    REQUIRE(itv);
    itv->parts.emplace_back(intersectionTypeId);

    CHECK(lookupProp(intersectionTypeId, "RandomProp").empty());
}

TEST_CASE_FIXTURE(Fixture, "lookupProp returns multiple results for union types")
{
    // Use check() which uses the internal Luau parser
    check(R"(
        type A = { prop: number }
        type B = { prop: string }
        type Union = A | B
        local x: Union = {} :: any
    )");

    auto module = getMainModule();
    REQUIRE(module);

    auto ty = requireType("x");
    auto results = lookupProp(Luau::follow(ty), "prop");
    CHECK_EQ(results.size(), 2);
}

TEST_CASE_FIXTURE(Fixture, "lookupProp returns multiple results for union of same generic instantiation")
{
    // Use check() which uses the internal Luau parser
    check(R"(
        type Base<T> = { prop: T }
        type Union = Base<number> | Base<string>
        local x: Union = {} :: any
    )");

    auto module = getMainModule();
    REQUIRE(module);

    auto ty = requireType("x");
    auto results = lookupProp(Luau::follow(ty), "prop");
    // Both union options have the property, so we get 2 results
    // (even though they're from the same generic definition)
    CHECK_EQ(results.size(), 2);
}

TEST_SUITE_END();
