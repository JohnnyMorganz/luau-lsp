#include "doctest.h"
#include "Fixture.h"
#include "LSP/IostreamHelpers.hpp"
#include "Platform/RobloxPlatform.hpp"

TEST_SUITE_BEGIN("FileRename");

TEST_CASE_FIXTURE(Fixture, "updates_relative_require_on_rename")
{
    // Enable the feature and use standard platform for string requires
    client->globalConfig.updateRequiresOnFileMove.enabled = UpdateRequiresOnFileMoveConfig::Always;
    // HACK: Fixture is loaded for RobloxPlatform
    client->globalConfig.platform.type = LSPPlatformConfig::Standard;
    workspace.appliedFirstTimeConfiguration = false;
    workspace.setupWithConfiguration(client->globalConfig);

    // Use .lua extension since virtual documents don't exist on disk
    // and the resolver falls back to .lua when .luau doesn't exist
    auto moduleA = newDocument("ModuleA.lua", R"(
return {}
)");

    auto moduleB = newDocument("ModuleB.lua", R"(
local A = require("./ModuleA")
return {}
)");

    // Type-check modules to build dependency graph
    auto moduleAName = workspace.fileResolver.getModuleName(moduleA);
    auto moduleBName = workspace.fileResolver.getModuleName(moduleB);

    workspace.frontend.check(moduleBName);

    // Simulate renaming ModuleA to RenamedModule
    std::vector<lsp::FileRename> renames;
    renames.push_back(lsp::FileRename{moduleA, Uri::file(workspace.rootUri.fsPath() + "/RenamedModule.lua")});

    auto edit = workspace.onWillRenameFiles(renames);

    REQUIRE_EQ(edit.changes.size(), 1);
    REQUIRE(edit.changes.count(moduleB));
    REQUIRE_EQ(edit.changes.at(moduleB).size(), 1);
    CHECK(edit.changes.at(moduleB)[0].newText.find("RenamedModule") != std::string::npos);
}

TEST_CASE_FIXTURE(Fixture, "updates_require_when_file_moved_to_subdirectory")
{
    // Enable the feature and use standard platform for string requires
    client->globalConfig.updateRequiresOnFileMove.enabled = UpdateRequiresOnFileMoveConfig::Always;
    // HACK: Fixture is loaded for RobloxPlatform
    client->globalConfig.platform.type = LSPPlatformConfig::Standard;
    workspace.appliedFirstTimeConfiguration = false;
    workspace.setupWithConfiguration(client->globalConfig);

    auto moduleA = newDocument("ModuleA.lua", R"(
return {}
)");

    auto moduleB = newDocument("ModuleB.lua", R"(
local A = require("./ModuleA")
return {}
)");

    // Type-check to build dependency graph
    workspace.frontend.check(workspace.fileResolver.getModuleName(moduleB));

    // Simulate moving ModuleA to a subdirectory
    std::vector<lsp::FileRename> renames;
    renames.push_back(lsp::FileRename{moduleA, Uri::file(workspace.rootUri.fsPath() + "/subdir/ModuleA.lua")});

    auto edit = workspace.onWillRenameFiles(renames);

    REQUIRE_EQ(edit.changes.size(), 1);
    REQUIRE(edit.changes.count(moduleB));
    REQUIRE_EQ(edit.changes.at(moduleB).size(), 1);
    CHECK(edit.changes.at(moduleB)[0].newText.find("subdir") != std::string::npos);
}

TEST_CASE_FIXTURE(Fixture, "updates_multiple_requires_in_same_file")
{
    // Enable the feature and use standard platform for string requires
    client->globalConfig.updateRequiresOnFileMove.enabled = UpdateRequiresOnFileMoveConfig::Always;
    // HACK: Fixture is loaded for RobloxPlatform
    client->globalConfig.platform.type = LSPPlatformConfig::Standard;
    workspace.appliedFirstTimeConfiguration = false;
    workspace.setupWithConfiguration(client->globalConfig);

    auto moduleA = newDocument("ModuleA.lua", R"(
return {}
)");

    auto moduleB = newDocument("ModuleB.lua", R"(
local A = require("./ModuleA")
local A2 = require("./ModuleA")
return {}
)");

    // Type-check to build dependency graph
    workspace.frontend.check(workspace.fileResolver.getModuleName(moduleB));

    std::vector<lsp::FileRename> renames;
    renames.push_back(lsp::FileRename{moduleA, Uri::file(workspace.rootUri.fsPath() + "/RenamedModule.lua")});

    auto edit = workspace.onWillRenameFiles(renames);

    REQUIRE_EQ(edit.changes.size(), 1);
    REQUIRE(edit.changes.count(moduleB));
    CHECK_EQ(edit.changes.at(moduleB).size(), 2);
}

TEST_CASE_FIXTURE(Fixture, "updates_requires_in_multiple_dependent_files")
{
    // Enable the feature and use standard platform for string requires
    client->globalConfig.updateRequiresOnFileMove.enabled = UpdateRequiresOnFileMoveConfig::Always;
    // HACK: Fixture is loaded for RobloxPlatform
    client->globalConfig.platform.type = LSPPlatformConfig::Standard;
    workspace.appliedFirstTimeConfiguration = false;
    workspace.setupWithConfiguration(client->globalConfig);

    auto moduleA = newDocument("ModuleA.lua", R"(
return {}
)");

    auto moduleB = newDocument("ModuleB.lua", R"(
local A = require("./ModuleA")
return {}
)");

    auto moduleC = newDocument("ModuleC.lua", R"(
local A = require("./ModuleA")
return {}
)");

    // Type-check to build dependency graph
    workspace.frontend.check(workspace.fileResolver.getModuleName(moduleB));
    workspace.frontend.check(workspace.fileResolver.getModuleName(moduleC));

    std::vector<lsp::FileRename> renames;
    renames.push_back(lsp::FileRename{moduleA, Uri::file(workspace.rootUri.fsPath() + "/RenamedModule.lua")});

    auto edit = workspace.onWillRenameFiles(renames);

    REQUIRE_EQ(edit.changes.size(), 2);
    CHECK(edit.changes.count(moduleB));
    CHECK(edit.changes.count(moduleC));
}

TEST_CASE_FIXTURE(Fixture, "does_not_update_unrelated_requires")
{
    // Enable the feature and use standard platform for string requires
    client->globalConfig.updateRequiresOnFileMove.enabled = UpdateRequiresOnFileMoveConfig::Always;
    // HACK: Fixture is loaded for RobloxPlatform
    client->globalConfig.platform.type = LSPPlatformConfig::Standard;
    workspace.appliedFirstTimeConfiguration = false;
    workspace.setupWithConfiguration(client->globalConfig);

    auto moduleA = newDocument("ModuleA.lua", R"(
return {}
)");

    auto moduleX = newDocument("ModuleX.lua", R"(
return {}
)");

    auto moduleB = newDocument("ModuleB.lua", R"(
local X = require("./ModuleX")
return {}
)");

    // Type-check to build dependency graph
    workspace.frontend.check(workspace.fileResolver.getModuleName(moduleB));

    // Rename ModuleA, should not affect ModuleB which requires ModuleX
    std::vector<lsp::FileRename> renames;
    renames.push_back(lsp::FileRename{moduleA, Uri::file(workspace.rootUri.fsPath() + "/RenamedModule.lua")});

    auto edit = workspace.onWillRenameFiles(renames);

    CHECK_EQ(edit.changes.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "ignores_non_luau_files")
{
    // Enable the feature and use standard platform
    client->globalConfig.updateRequiresOnFileMove.enabled = UpdateRequiresOnFileMoveConfig::Always;
    // HACK: Fixture is loaded for RobloxPlatform
    client->globalConfig.platform.type = LSPPlatformConfig::Standard;
    workspace.appliedFirstTimeConfiguration = false;
    workspace.setupWithConfiguration(client->globalConfig);

    // Only .lua and .luau files should be processed
    std::vector<lsp::FileRename> renames;
    renames.push_back(lsp::FileRename{Uri::file("/some/path/file.txt"), Uri::file("/some/path/renamed.txt")});

    auto edit = workspace.onWillRenameFiles(renames);

    CHECK_EQ(edit.changes.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "updates_instance_require_on_rename")
{
    // Enable the feature
    client->globalConfig.updateRequiresOnFileMove.enabled = UpdateRequiresOnFileMoveConfig::Always;

    // Set up sourcemap with ReplicatedStorage folder containing ModuleA and ModuleB
    loadSourcemap(R"({
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "filePaths": ["."],
                "children": [
                    {"name": "ModuleA", "className": "ModuleScript", "filePaths": ["ModuleA.lua"]},
                    {"name": "ModuleB", "className": "ModuleScript", "filePaths": ["ModuleB.lua"]}
                ]
            }
        ]
    })");

    auto moduleA = newDocument("ModuleA.lua", "return {}");
    auto moduleB = newDocument("ModuleB.lua", "local A = require(script.Parent.ModuleA)\nreturn {}");

    // Type-check to build dependency graph
    workspace.frontend.check("game/ReplicatedStorage/ModuleB");

    // Simulate renaming ModuleA to RenamedModule
    auto newUri = Uri::file(workspace.rootUri.fsPath() + "/RenamedModule.lua");
    std::vector<lsp::FileRename> renames;
    renames.push_back(lsp::FileRename{moduleA, newUri});

    auto edit = workspace.onWillRenameFiles(renames);

    REQUIRE_EQ(edit.changes.size(), 1);
    REQUIRE(edit.changes.count(moduleB));
    REQUIRE_EQ(edit.changes.at(moduleB).size(), 1);
    // Should generate something like "script.Parent.RenamedModule"
    CHECK(edit.changes.at(moduleB)[0].newText.find("RenamedModule") != std::string::npos);
}

TEST_CASE_FIXTURE(Fixture, "updates_instance_require_when_moved_to_child_folder")
{
    // Enable the feature
    client->globalConfig.updateRequiresOnFileMove.enabled = UpdateRequiresOnFileMoveConfig::Always;

    // Set up sourcemap with ReplicatedStorage containing ModuleA, ModuleB, and a Subfolder
    loadSourcemap(R"({
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "filePaths": ["."],
                "children": [
                    {"name": "ModuleA", "className": "ModuleScript", "filePaths": ["ModuleA.lua"]},
                    {"name": "ModuleB", "className": "ModuleScript", "filePaths": ["ModuleB.lua"]},
                    {"name": "Subfolder", "className": "Folder", "filePaths": ["Subfolder"]}
                ]
            }
        ]
    })");

    auto moduleA = newDocument("ModuleA.lua", "return {}");
    auto moduleB = newDocument("ModuleB.lua", "local A = require(script.Parent.ModuleA)\nreturn {}");

    // Type-check to build dependency graph
    workspace.frontend.check("game/ReplicatedStorage/ModuleB");

    // Simulate moving ModuleA to a subfolder
    auto newUri = Uri::file(workspace.rootUri.fsPath() + "/Subfolder/ModuleA.lua");
    std::vector<lsp::FileRename> renames;
    renames.push_back(lsp::FileRename{moduleA, newUri});

    auto edit = workspace.onWillRenameFiles(renames);

    REQUIRE_EQ(edit.changes.size(), 1);
    REQUIRE(edit.changes.count(moduleB));
    REQUIRE_EQ(edit.changes.at(moduleB).size(), 1);
    // Should generate something like "script.Parent.Subfolder.ModuleA"
    CHECK(edit.changes.at(moduleB)[0].newText.find("Subfolder") != std::string::npos);
}

TEST_SUITE_END();
