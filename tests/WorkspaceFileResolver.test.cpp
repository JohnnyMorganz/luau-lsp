#include "doctest.h"
#include "LSP/WorkspaceFileResolver.hpp"
#include "Luau/Ast.h"
#include "Luau/FileResolver.h"

TEST_SUITE_BEGIN("WorkspaceFileResolverTests");

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

TEST_CASE("resolveModule handles FindFirstChild")
{
    WorkspaceFileResolver fileResolver;

    Luau::ModuleInfo baseContext{"game/ReplicatedStorage"};

    // :FindFirstChild("Testing")
    std::string tempString = "Testing";
    Luau::AstArray<char> testingStr{tempString.data(), tempString.size()};
    std::vector<Luau::AstExpr*> tempArgs{Luau::AstExprConstantString(Luau::Location(), testingStr).asExpr()};
    Luau::AstArray<Luau::AstExpr*> args{tempArgs.data(), tempArgs.size()};
    auto expr = Luau::AstExprCall(Luau::Location(),
        Luau::AstExprIndexName(Luau::Location(), nullptr, Luau::AstName("FindFirstChild"), Luau::Location(), Luau::Position(0, 0), ':').asExpr(),
        args, true, Luau::Location());
    auto resolved = fileResolver.resolveModule(&baseContext, &expr);

    REQUIRE(resolved.has_value());
    CHECK_EQ(resolved->name, "game/ReplicatedStorage/Testing");
}

TEST_CASE("resolveModule fails on FindFirstChild with recursive enabled")
{
    WorkspaceFileResolver fileResolver;

    Luau::ModuleInfo baseContext{"game/Players/LocalPlayer/StarterGear"};

    // :FindFirstChild("Testing", true)
    std::string tempString = "Testing";
    Luau::AstArray<char> testingStr{tempString.data(), tempString.size()};
    std::vector<Luau::AstExpr*> tempArgs{
        Luau::AstExprConstantString(Luau::Location(), testingStr).asExpr(), Luau::AstExprConstantBool(Luau::Location(), true).asExpr()};
    Luau::AstArray<Luau::AstExpr*> args{tempArgs.data(), tempArgs.size()};
    auto expr = Luau::AstExprCall(Luau::Location(),
        Luau::AstExprIndexName(Luau::Location(), nullptr, Luau::AstName("FindFirstChild"), Luau::Location(), Luau::Position(0, 0), ':').asExpr(),
        args, true, Luau::Location());
    auto resolved = fileResolver.resolveModule(&baseContext, &expr);

    CHECK_FALSE(resolved.has_value());
}

TEST_SUITE_END();