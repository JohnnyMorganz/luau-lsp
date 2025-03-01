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

TEST_CASE("support_disabling_global_types")
{
    auto client = std::make_shared<Client>(Client{});
    auto workspace = WorkspaceFolder(client, "$TEST_WORKSPACE", Uri(), std::nullopt);

    auto config = defaultTestClientConfiguration();
    config.types.disabledGlobals = {
        "table",
    };

    workspace.setupWithConfiguration(config);

    auto document = newDocument(workspace, "foo.luau", R"(
        local x = string.split("", "")
        local y = table.insert({}, 1)
    )");

    auto result = workspace.frontend.check("foo.luau");
    REQUIRE_EQ(result.errors.size(), 1);

    auto err = Luau::get<Luau::UnknownSymbol>(result.errors[0]);
    REQUIRE(err);
    CHECK_EQ(err->name, "table");
    CHECK_EQ(err->context, Luau::UnknownSymbol::Context::Binding);
}

TEST_CASE("support_disabling_methods_in_global_types")
{
    auto client = std::make_shared<Client>(Client{});
    auto workspace = WorkspaceFolder(client, "$TEST_WORKSPACE", Uri(), std::nullopt);

    auto config = defaultTestClientConfiguration();
    config.types.disabledGlobals = {
        "table.insert",
    };

    workspace.setupWithConfiguration(config);

    auto document = newDocument(workspace, "foo.luau", R"(
        local x = table.find({}, "value")
        local y = table.insert({}, 1)
    )");

    auto result = workspace.frontend.check("foo.luau");
    REQUIRE_EQ(result.errors.size(), 1);

    auto err = Luau::get<Luau::UnknownProperty>(result.errors[0]);
    REQUIRE(err);
    CHECK_EQ(Luau::toString(err->table), "typeof(table)");
    CHECK_EQ(err->key, "insert");
}

TEST_SUITE_END();
