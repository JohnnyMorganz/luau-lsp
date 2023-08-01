#include "doctest.h"
#include "Fixture.h"
#include "LSP/WorkspaceFileResolver.hpp"
#include "Luau/Ast.h"
#include "Luau/FileResolver.h"

TEST_SUITE_BEGIN("WorkspaceFileResolverTests");

Luau::AstArray<char> writeAstArray(const std::string& ss)
{
    Luau::AstArray<char> value;
    value.data = const_cast<char*>(ss.c_str());
    value.size = strlen(value.data);
    return value;
}


TEST_CASE("resolveModule handles LocalPlayer PlayerScripts")
{
    WorkspaceFileResolver fileResolver;

    Luau::ModuleInfo baseContext{"game/Players/LocalPlayer/PlayerScripts"};
    auto expr = Luau::AstExprIndexName(Luau::Location(), nullptr, Luau::AstName("PurchaseClient"), Luau::Location(), Luau::Position(0, 0), '.');
    auto resolved = fileResolver.resolveModule(&baseContext, &expr);

    REQUIRE(resolved.has_value());
    CHECK_EQ(resolved->name, "game/StarterPlayer/StarterPlayerScripts/PurchaseClient");
}

TEST_CASE("resolveModule handles LocalPlayer PlayerGui")
{
    WorkspaceFileResolver fileResolver;

    Luau::ModuleInfo baseContext{"game/Players/LocalPlayer/PlayerGui"};
    auto expr = Luau::AstExprIndexName(Luau::Location(), nullptr, Luau::AstName("GuiScript"), Luau::Location(), Luau::Position(0, 0), '.');
    auto resolved = fileResolver.resolveModule(&baseContext, &expr);

    REQUIRE(resolved.has_value());
    CHECK_EQ(resolved->name, "game/StarterGui/GuiScript");
}

TEST_CASE("resolveModule handles LocalPlayer StarterGear")
{
    WorkspaceFileResolver fileResolver;

    Luau::ModuleInfo baseContext{"game/Players/LocalPlayer/StarterGear"};
    auto expr = Luau::AstExprIndexName(Luau::Location(), nullptr, Luau::AstName("GearScript"), Luau::Location(), Luau::Position(0, 0), '.');
    auto resolved = fileResolver.resolveModule(&baseContext, &expr);

    REQUIRE(resolved.has_value());
    CHECK_EQ(resolved->name, "game/StarterPack/GearScript");
}

TEST_CASE_FIXTURE(Fixture, "resolveModule handles FindFirstChild")
{
    WorkspaceFileResolver fileResolver;

    Luau::ModuleInfo baseContext{"game/ReplicatedStorage"};

    Luau::AstStatBlock* block = parse(R"(
        local _ = node:FindFirstChild("Testing")
    )");
    REQUIRE(block != nullptr);
    REQUIRE(block->body.size > 0);

    Luau::AstStatLocal* local = block->body.data[0]->as<Luau::AstStatLocal>();
    REQUIRE(local != nullptr);
    REQUIRE_EQ(1, local->values.size);

    auto resolved = fileResolver.resolveModule(&baseContext, local->values.data[0]);

    REQUIRE(resolved.has_value());
    CHECK_EQ(resolved->name, "game/ReplicatedStorage/Testing");
}

TEST_CASE_FIXTURE(Fixture, "resolveModule fails on FindFirstChild with recursive enabled")
{
    WorkspaceFileResolver fileResolver;

    Luau::ModuleInfo baseContext{"game/ReplicatedStorage"};

    Luau::AstStatBlock* block = parse(R"(
        local _ = node:FindFirstChild("Testing", true)
    )");
    REQUIRE(block != nullptr);
    REQUIRE(block->body.size > 0);

    Luau::AstStatLocal* local = block->body.data[0]->as<Luau::AstStatLocal>();
    REQUIRE(local != nullptr);
    REQUIRE_EQ(1, local->values.size);

    auto resolved = fileResolver.resolveModule(&baseContext, local->values.data[0]);

    CHECK_FALSE(resolved.has_value());
}

TEST_CASE_FIXTURE(Fixture, "resolveDirectoryAliases")
{
    std::unordered_map<std::string, std::string> directoryAliases{
        {"@test1/", "C:/Users/test/test1"},
        {"@test2/", "~/test2"},
    };

    auto home = getHomeDirectory();
    REQUIRE(home);

    CHECK_EQ(resolveDirectoryAlias(directoryAliases, "@test1/foo"), "C:/Users/test/test1/foo.lua");
    CHECK_EQ(resolveDirectoryAlias(directoryAliases, "@test1/foo.luau"), "C:/Users/test/test1/foo.luau");
    CHECK_EQ(resolveDirectoryAlias(directoryAliases, "@test1/", /* includeExtension = */ false), "C:/Users/test/test1");

    CHECK_EQ(resolveDirectoryAlias(directoryAliases, "@test2/bar"), home.value() / "test2" / "bar.lua");

    CHECK_EQ(resolveDirectoryAlias(directoryAliases, "@test3/bar"), std::nullopt);
}

TEST_CASE_FIXTURE(Fixture, "string require doesn't add file extension if already exists")
{
    WorkspaceFileResolver fileResolver;

    Luau::ModuleInfo baseContext{};
    auto expr = Luau::AstExprConstantString(Luau::Location(), writeAstArray("Module.luau"));
    auto resolved = fileResolver.resolveModule(&baseContext, &expr);

    REQUIRE(resolved.has_value());
    CHECK_EQ(resolved->name, "/Module.luau");
}

TEST_CASE_FIXTURE(Fixture, "string require doesn't replace a non-luau/lua extension")
{
    WorkspaceFileResolver fileResolver;

    Luau::ModuleInfo baseContext{};
    auto expr = Luau::AstExprConstantString(Luau::Location(), writeAstArray("Module.mod"));
    auto resolved = fileResolver.resolveModule(&baseContext, &expr);

    REQUIRE(resolved.has_value());
    CHECK_EQ(resolved->name, "/Module.mod.lua");
}

TEST_SUITE_END();