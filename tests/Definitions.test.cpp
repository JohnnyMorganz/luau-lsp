#include "doctest.h"
#include "Fixture.h"
#include "Platform/RobloxPlatform.hpp"

using namespace Luau::LanguageServer;

TEST_SUITE_BEGIN("Definitions");

TEST_CASE("use_platform_metadata_from_first_registered_definitions_file")
{
    auto client = std::make_shared<Client>(Client{});
    auto workspace = WorkspaceFolder(client, "$TEST_WORKSPACE", Uri(), std::nullopt);

    client->definitionsFiles.emplace("@roblox", "./tests/testdata/standard_definitions.d.luau");
    client->definitionsFiles.emplace("@roblox1", "./tests/testdata/extra_definitions_relying_on_mutations.d.luau");

    workspace.setupWithConfiguration(defaultTestClientConfiguration());

    REQUIRE(workspace.definitionsFileMetadata);

    RobloxDefinitionsFileMetadata metadata = workspace.definitionsFileMetadata.value();
    REQUIRE(!metadata.SERVICES.empty());
    REQUIRE(!metadata.CREATABLE_INSTANCES.empty());
}

TEST_CASE("handles_definitions_files_relying_on_mutations")
{
    auto client = std::make_shared<Client>(Client{});
    auto workspace = WorkspaceFolder(client, "$TEST_WORKSPACE", Uri(), std::nullopt);

    client->definitionsFiles.emplace("@roblox", "./tests/testdata/standard_definitions.d.luau");
    client->definitionsFiles.emplace("@roblox1", "./tests/testdata/extra_definitions_relying_on_mutations.d.luau");

    workspace.setupWithConfiguration(defaultTestClientConfiguration());

    auto document = newDocument(workspace, "foo.luau", R"(
        local x: ExtraDataRelyingOnMutations
        local y = x.RigType
    )");

    auto result = workspace.frontend.check("foo.luau");
    REQUIRE(result.errors.empty());
}

TEST_CASE("package_name_is_recorded_onto_the_loaded_types")
{
    auto client = std::make_shared<Client>(Client{});
    auto workspace = WorkspaceFolder(client, "$TEST_WORKSPACE", Uri(), std::nullopt);

    client->definitionsFiles.emplace("@example", "./tests/testdata/standard_definitions.d.luau");

    workspace.setupWithConfiguration(defaultTestClientConfiguration());

    auto document = newDocument(workspace, "foo.luau", R"(
        local x: Instance
    )");

    auto result = workspace.frontend.check("foo.luau");
    auto module = workspace.frontend.moduleResolver.getModule("foo.luau");
    REQUIRE(module);

    auto binding = module->getModuleScope()->linearSearchForBinding("x");
    REQUIRE(binding);

    auto ty = Luau::follow(binding->typeId);
    CHECK_EQ(ty->documentationSymbol, "@example/globaltype/Instance");

    auto ctv = Luau::get<Luau::ClassType>(ty);
    REQUIRE(ctv);
    CHECK_EQ(ctv->definitionModuleName, "@example");
}

TEST_SUITE_END();
