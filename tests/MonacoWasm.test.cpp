#include "doctest.h"
#include "Fixture.h"

#include "LSP/IostreamHelpers.hpp"
#include "LSP/ClientConfiguration.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"
#include "Luau/BuiltinDefinitions.h"
#include "Protocol/Structures.hpp"
#include "LSP/LuauExt.hpp"

TEST_SUITE_BEGIN("MonacoWasm");

TEST_CASE_FIXTURE(Fixture, "monaco_test_one")
{
    ClientConfiguration config;
    config.sourcemap.enabled = false;
    config.index.enabled = false;
    ClientPlatformConfiguration platform;
    platform.type = LSPPlatformConfig::Monaco;
    config.platform = platform;
    workspace.setupWithConfiguration(config);
    std::string filename = "MainModule";
    Uri uri("inmemory", "model", "/" + filename);
    auto source = R"(
        local T = {}
        T.boof = "testing"
    )";
    workspace.openTextDocument(uri, {{uri, "luau", 0, source}});
    auto result = workspace.frontend.check(uri.toString());
    REQUIRE_EQ(0, result.errors.size());

    auto module = workspace.frontend.moduleResolver.getModule(uri.toString());
    REQUIRE(module);
    REQUIRE(module->hasModuleScope());
    auto scope = module->getModuleScope();
    REQUIRE(scope);
    std::optional<Luau::Binding> binding = scope->linearSearchForBinding("T");
    REQUIRE(binding);
    auto ty = Luau::follow(binding->typeId);
    auto references = workspace.findAllReferences(ty, "boof");
    REQUIRE_EQ(1, references.size());
    CHECK(references[0].location.begin.line == 2);
    CHECK(references[0].location.begin.column == 10);
    CHECK(references[0].location.end.line == 2);
    CHECK(references[0].location.end.column == 14);
}