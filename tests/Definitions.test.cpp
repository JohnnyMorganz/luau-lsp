#include "doctest.h"
#include "Fixture.h"
#include "Platform/RobloxPlatform.hpp"

using namespace Luau::LanguageServer;

TEST_SUITE_BEGIN("Definitions");

TEST_CASE("use_platform_metadata_from_first_registered_definitions_file")
{
    auto client = std::make_shared<Client>(Client{});
    auto workspace = WorkspaceFolder(client, "$TEST_WORKSPACE", Uri(), std::nullopt);

    client->definitionsFiles.emplace_back("./tests/testdata/standard_definitions.d.luau");
    client->definitionsFiles.emplace_back("./tests/testdata/extra_definitions_relying_on_mutations.d.luau");

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

    client->definitionsFiles.emplace_back("./tests/testdata/standard_definitions.d.luau");
    client->definitionsFiles.emplace_back("./tests/testdata/extra_definitions_relying_on_mutations.d.luau");

    workspace.setupWithConfiguration(defaultTestClientConfiguration());

    auto document = newDocument(workspace, "foo.luau", R"(
        local x: ExtraDataRelyingOnMutations
        local y = x.RigType
    )");

    auto result = workspace.frontend.check("foo.luau");
    REQUIRE(result.errors.empty());
}

TEST_SUITE_END();
