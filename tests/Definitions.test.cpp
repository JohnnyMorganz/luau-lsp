#include "doctest.h"
#include "Fixture.h"
#include "TempDir.h"
#include "Platform/RobloxPlatform.hpp"
#include "LuauFileUtils.hpp"
#include "Luau/Parser.h"
#include "Protocol/Workspace.hpp"

using namespace Luau::LanguageServer;

TEST_SUITE_BEGIN("Definitions");

TEST_CASE("use_platform_metadata_from_first_registered_definitions_file")
{
    TestClient client;
    auto workspace = WorkspaceFolder(&client, "$TEST_WORKSPACE", Uri(), std::nullopt);

    client.definitionsFiles.emplace("@roblox", "./tests/testdata/standard_definitions.d.luau");
    client.definitionsFiles.emplace("@roblox1", "./tests/testdata/extra_definitions_relying_on_mutations.d.luau");

    workspace.setupWithConfiguration(defaultTestClientConfiguration());
    workspace.isReady = true;

    REQUIRE(workspace.definitionsFileMetadata);

    RobloxDefinitionsFileMetadata metadata = workspace.definitionsFileMetadata.value();
    REQUIRE(!metadata.SERVICES.empty());
    REQUIRE(!metadata.CREATABLE_INSTANCES.empty());
}

TEST_CASE("handles_definitions_files_relying_on_mutations")
{
    TestClient client;
    auto workspace = WorkspaceFolder(&client, "$TEST_WORKSPACE", Uri::file(*Luau::FileUtils::getCurrentWorkingDirectory()), std::nullopt);

    client.definitionsFiles.emplace("@roblox", "./tests/testdata/standard_definitions.d.luau");
    client.definitionsFiles.emplace("@roblox1", "./tests/testdata/extra_definitions_relying_on_mutations.d.luau");

    workspace.setupWithConfiguration(defaultTestClientConfiguration());
    workspace.isReady = true;

    auto document = newDocument(workspace, "foo.luau", R"(
        local x: ExtraDataRelyingOnMutations
        local y = x.RigType
    )");

    auto result = workspace.frontend.check(workspace.fileResolver.getModuleName(document));
    REQUIRE(result.errors.empty());
}

TEST_CASE("dont_crash_when_mutating_a_definitions_file_that_does_not_contain_expected_state")
{
    TestClient client;
    auto workspace = WorkspaceFolder(&client, "$TEST_WORKSPACE", Uri(), std::nullopt);

    client.definitionsFiles.emplace("@roblox", "./tests/testdata/bad_standard_definitions.d.luau");

    workspace.setupWithConfiguration(defaultTestClientConfiguration());

    REQUIRE(workspace.definitionsFileMetadata);
}

TEST_CASE("support_disabling_global_types")
{
    TestClient client;
    auto workspace = WorkspaceFolder(&client, "$TEST_WORKSPACE", Uri::file(*Luau::FileUtils::getCurrentWorkingDirectory()), std::nullopt);

    auto config = defaultTestClientConfiguration();
    config.types.disabledGlobals = {
        "table",
    };

    workspace.setupWithConfiguration(config);
    workspace.isReady = true;

    auto document = newDocument(workspace, "foo.luau", R"(
        --!strict
        local x = string.split("", "")
        local y = table.insert({}, 1)
    )");

    auto result = workspace.frontend.check(workspace.fileResolver.getModuleName(document));
    REQUIRE_EQ(result.errors.size(), 1);

    auto err = Luau::get<Luau::UnknownSymbol>(result.errors[0]);
    REQUIRE(err);
    CHECK_EQ(err->name, "table");
    CHECK_EQ(err->context, Luau::UnknownSymbol::Context::Binding);
}

TEST_CASE("support_disabling_methods_in_global_types")
{
    TestClient client;
    auto workspace = WorkspaceFolder(&client, "$TEST_WORKSPACE", Uri::file(*Luau::FileUtils::getCurrentWorkingDirectory()), std::nullopt);

    auto config = defaultTestClientConfiguration();
    config.types.disabledGlobals = {
        "table.insert",
    };

    workspace.setupWithConfiguration(config);
    workspace.isReady = true;

    auto document = newDocument(workspace, "foo.luau", R"(
        --!strict
        local x = table.find({}, "value")
        local y = table.insert({}, 1)
    )");

    auto result = workspace.frontend.check(workspace.fileResolver.getModuleName(document));
    REQUIRE_EQ(result.errors.size(), 1);

    auto err = Luau::get<Luau::UnknownProperty>(result.errors[0]);
    REQUIRE(err);
    CHECK_EQ(Luau::toString(err->table), "typeof(table)");
    CHECK_EQ(err->key, "insert");
}

TEST_CASE("package_name_is_recorded_onto_the_loaded_types")
{
    TestClient client;
    auto workspace = WorkspaceFolder(&client, "$TEST_WORKSPACE", Uri::file(*Luau::FileUtils::getCurrentWorkingDirectory()), std::nullopt);

    client.definitionsFiles.emplace("@example", "./tests/testdata/standard_definitions.d.luau");

    workspace.setupWithConfiguration(defaultTestClientConfiguration());
    workspace.isReady = true;

    auto document = newDocument(workspace, "foo.luau", R"(
        local x: Instance
    )");

    auto result = workspace.frontend.check(workspace.fileResolver.getModuleName(document));
    auto module = workspace.frontend.moduleResolver.getModule(workspace.fileResolver.getModuleName(document));
    REQUIRE(module);

    auto binding = module->getModuleScope()->linearSearchForBinding("x");
    REQUIRE(binding);

    auto ty = Luau::follow(binding->typeId);
    CHECK_EQ(ty->documentationSymbol, "@example/globaltype/Instance");

    auto ctv = Luau::get<Luau::ExternType>(ty);
    REQUIRE(ctv);
    CHECK_EQ(ctv->definitionModuleName, "@example");
}

TEST_CASE("support_disabling_methods_in_extern_types_globals")
{
    TestClient client;
    auto workspace = WorkspaceFolder(&client, "$TEST_WORKSPACE", Uri::file(*Luau::FileUtils::getCurrentWorkingDirectory()), std::nullopt);

    client.definitionsFiles.emplace("@roblox", "./tests/testdata/standard_definitions.d.luau");

    auto config = defaultTestClientConfiguration();
    config.types.disabledGlobals = {
        "game.BindToClose",
    };

    workspace.setupWithConfiguration(config);
    workspace.isReady = true;

    auto document = newDocument(workspace, "foo.luau", R"(
        --!strict
        game:BindToClose(function() end)
    )");

    auto result = workspace.frontend.check(workspace.fileResolver.getModuleName(document));
    REQUIRE_EQ(result.errors.size(), 1);

    auto err = Luau::get<Luau::UnknownProperty>(result.errors[0]);
    REQUIRE(err);
    CHECK_EQ(Luau::toString(err->table), "DataModel");
    CHECK_EQ(err->key, "BindToClose");
}

TEST_CASE_FIXTURE(Fixture, "type_functions_in_definition_files_work")
{
    ENABLE_NEW_SOLVER();

    loadDefinition("@test", R"(
        export type function foo(ty)
            return types.negationof(ty)
        end
    )");

    auto result = check(R"(
        local x: foo<number> = nil :: any
    )");
    REQUIRE(result.errors.empty());
}

TEST_CASE("hot_reload_reloads_global_definitions_when_enabled")
{
    TestClient client;
    TempDir tempDir("hot_reload_definitions");
    auto workspace = WorkspaceFolder(&client, "$TEST_WORKSPACE", Uri::file(tempDir.path()), std::nullopt);

    auto defPath = tempDir.write_child("extra.d.luau", R"(
        declare extern type HotReloadType with
            prop: number
        end
    )");

    client.definitionsFiles.emplace("@hotreload", defPath);

    auto config = defaultTestClientConfiguration();
    config.platform.type = LSPPlatformConfig::Standard;
    config.types.definitionFiles = {{"@hotreload", defPath}};
    config.types.hotReload = true;
    client.globalConfig = config;

    workspace.setupWithConfiguration(config);
    workspace.isReady = true;

    auto document = newDocument(workspace, "foo.luau", R"(
        --!strict
        local x: HotReloadType = nil :: any
        local _y = x.prop
    )");

    auto moduleName = workspace.fileResolver.getModuleName(document);
    auto result = workspace.frontend.check(moduleName);
    REQUIRE(result.errors.empty());

    tempDir.write_child("extra.d.luau", R"(
        declare extern type HotReloadType with
            prop: number
            newProp: string
        end
    )");

    lsp::FileEvent event{Uri::file(defPath), lsp::FileChangeType::Changed};
    workspace.onDidChangeWatchedFiles({event});

    updateDocument(workspace, document, R"(
        --!strict
        local x: HotReloadType = nil :: any
        local _y = x.newProp
    )");

    result = workspace.frontend.check(moduleName);
    CHECK(result.errors.empty());
}

TEST_CASE("hot_reload_ignored_on_roblox_platform")
{
    TestClient client;
    TempDir tempDir("hot_reload_definitions_roblox");
    auto workspace = WorkspaceFolder(&client, "$TEST_WORKSPACE", Uri::file(tempDir.path()), std::nullopt);

    auto defPath = tempDir.write_child("extra.d.luau", R"(
        declare extern type HotReloadType with
            prop: number
        end
    )");

    client.definitionsFiles.emplace("@hotreload", defPath);

    auto config = defaultTestClientConfiguration();
    config.platform.type = LSPPlatformConfig::Roblox;
    config.types.definitionFiles = {{"@hotreload", defPath}};
    config.types.hotReload = true;
    client.globalConfig = config;

    workspace.setupWithConfiguration(config);
    workspace.isReady = true;

    auto document = newDocument(workspace, "foo.luau", R"(
        --!strict
        local x: HotReloadType = nil :: any
        local _y = x.prop
    )");

    auto moduleName = workspace.fileResolver.getModuleName(document);
    REQUIRE(workspace.frontend.check(moduleName).errors.empty());

    tempDir.write_child("extra.d.luau", R"(
        declare extern type HotReloadType with
            prop: number
            newProp: string
        end
    )");

    client.notificationQueue.clear();
    lsp::FileEvent event{Uri::file(defPath), lsp::FileChangeType::Changed};
    workspace.onDidChangeWatchedFiles({event});

    bool sawReloadPrompt = false;
    for (const auto& [method, params] : client.notificationQueue)
    {
        if (method == "window/showMessage" && params && params->contains("message"))
        {
            auto message = params->at("message").get<std::string>();
            if (message.find("Detected changes to global definitions files") != std::string::npos)
                sawReloadPrompt = true;
        }
    }
    CHECK(sawReloadPrompt);

    updateDocument(workspace, document, R"(
        --!strict
        local x: HotReloadType = nil :: any
        local _y = x.newProp
    )");

    auto result = workspace.frontend.check(moduleName);
    REQUIRE_FALSE(result.errors.empty());
}

TEST_CASE("hot_reload_disabled_keeps_stale_definitions_until_workspace_reload")
{
    TestClient client;
    TempDir tempDir("hot_reload_definitions_disabled");
    auto workspace = WorkspaceFolder(&client, "$TEST_WORKSPACE", Uri::file(tempDir.path()), std::nullopt);

    auto defPath = tempDir.write_child("extra.d.luau", R"(
        declare extern type HotReloadType with
            prop: number
        end
    )");

    client.definitionsFiles.emplace("@hotreload", defPath);

    auto config = defaultTestClientConfiguration();
    config.platform.type = LSPPlatformConfig::Standard;
    config.types.definitionFiles = {{"@hotreload", defPath}};
    config.types.hotReload = false;
    client.globalConfig = config;

    workspace.setupWithConfiguration(config);
    workspace.isReady = true;

    auto document = newDocument(workspace, "foo.luau", R"(
        --!strict
        local x: HotReloadType = nil :: any
        local _y = x.prop
    )");

    auto moduleName = workspace.fileResolver.getModuleName(document);
    REQUIRE(workspace.frontend.check(moduleName).errors.empty());

    tempDir.write_child("extra.d.luau", R"(
        declare extern type HotReloadType with
            prop: number
            newProp: string
        end
    )");

    client.notificationQueue.clear();
    lsp::FileEvent event{Uri::file(defPath), lsp::FileChangeType::Changed};
    workspace.onDidChangeWatchedFiles({event});

    bool sawReloadPrompt = false;
    for (const auto& [method, params] : client.notificationQueue)
    {
        if (method == "window/showMessage" && params && params->contains("message"))
        {
            auto message = params->at("message").get<std::string>();
            if (message.find("Detected changes to global definitions files") != std::string::npos)
                sawReloadPrompt = true;
        }
    }
    CHECK(sawReloadPrompt);

    updateDocument(workspace, document, R"(
        --!strict
        local x: HotReloadType = nil :: any
        local _y = x.newProp
    )");

    auto result = workspace.frontend.check(moduleName);
    REQUIRE_FALSE(result.errors.empty());
}

TEST_SUITE_END();
