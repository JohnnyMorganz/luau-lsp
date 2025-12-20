#include "doctest.h"
#include "Fixture.h"
#include "LSP/WorkspaceFileResolver.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "Luau/Ast.h"
#include "Luau/FileResolver.h"

TEST_SUITE_BEGIN("WorkspaceTest");

TEST_CASE_FIXTURE(Fixture, "managed_files_correctly_resolves_for_untitled_uris")
{
    Uri uri = Uri::parse("untitled:Untitled-1");
    workspace.openTextDocument(uri, {{uri, "luau", 0, "Hello, World!"}});

    auto textDocumentFromUri = workspace.fileResolver.getTextDocument(uri);
    REQUIRE(textDocumentFromUri);
    CHECK_EQ(textDocumentFromUri->getText(), "Hello, World!");

    auto moduleName = workspace.fileResolver.getModuleName(uri);
    auto textDocumentFromModuleName = workspace.fileResolver.getTextDocumentFromModuleName(moduleName);
    REQUIRE(textDocumentFromModuleName);
    CHECK_EQ(textDocumentFromModuleName->getText(), "Hello, World!");
    CHECK_EQ(textDocumentFromUri, textDocumentFromModuleName);
}

TEST_CASE_FIXTURE(Fixture, "managed_files_correctly_resolves_for_non_file_uris")
{
    Uri uri = Uri::parse("inmemory://model/1");
    workspace.openTextDocument(uri, {{uri, "luau", 0, "Hello, World!"}});

    auto textDocumentFromUri = workspace.fileResolver.getTextDocument(uri);
    REQUIRE(textDocumentFromUri);
    CHECK_EQ(textDocumentFromUri->getText(), "Hello, World!");

    auto moduleName = workspace.fileResolver.getModuleName(uri);
    auto textDocumentFromModuleName = workspace.fileResolver.getTextDocumentFromModuleName(moduleName);
    REQUIRE(textDocumentFromModuleName);
    CHECK_EQ(textDocumentFromModuleName->getText(), "Hello, World!");
    CHECK_EQ(textDocumentFromUri, textDocumentFromModuleName);
}

TEST_CASE_FIXTURE(Fixture, "managed_files_correctly_resolves_for_file_uris")
{
    Uri uri = Uri::parse("file:///source.luau");
    workspace.openTextDocument(uri, {{uri, "luau", 0, "Hello, World!"}});

    auto textDocumentFromUri = workspace.fileResolver.getTextDocument(uri);
    REQUIRE(textDocumentFromUri);
    CHECK_EQ(textDocumentFromUri->getText(), "Hello, World!");

    auto moduleName = workspace.fileResolver.getModuleName(uri);
    auto textDocumentFromModuleName = workspace.fileResolver.getTextDocumentFromModuleName(moduleName);
    REQUIRE(textDocumentFromModuleName);
    CHECK_EQ(textDocumentFromModuleName->getText(), "Hello, World!");
    CHECK_EQ(textDocumentFromUri, textDocumentFromModuleName);
}

TEST_CASE_FIXTURE(Fixture, "managed_files_correctly_resolves_for_virtual_paths")
{
    loadSourcemap(R"(
    {
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "children": [
                    {
                        "name": "File",
                        "className": "ModuleScript",
                        "filePaths": ["source.luau"]
                    }
                ]
            }
        ]
    })");

    Uri uri = workspace.rootUri.resolvePath("source.luau");
    workspace.openTextDocument(uri, {{uri, "luau", 0, "Hello, World!"}});

    auto textDocumentFromUri = workspace.fileResolver.getTextDocument(uri);
    REQUIRE(textDocumentFromUri);
    CHECK_EQ(textDocumentFromUri->getText(), "Hello, World!");

    auto moduleName = workspace.fileResolver.getModuleName(uri);
    CHECK_EQ(moduleName, "game/ReplicatedStorage/File");

    auto textDocumentFromModuleName = workspace.fileResolver.getTextDocumentFromModuleName(moduleName);
    REQUIRE(textDocumentFromModuleName);
    CHECK_EQ(textDocumentFromModuleName->getText(), "Hello, World!");
    CHECK_EQ(textDocumentFromUri, textDocumentFromModuleName);
}

TEST_CASE_FIXTURE(Fixture, "isIgnoredFile")
{
    client->globalConfig.ignoreGlobs = {"**/_Index/**"};

    CHECK_EQ(workspace.isIgnoredFile(workspace.rootUri.resolvePath("source.luau")), false);
    CHECK_EQ(workspace.isIgnoredFile(workspace.rootUri.resolvePath("Packages/_Index/source.luau")), true);

    // Test upper vs. lower case drive letter
#ifdef _WIN32
    auto path = workspace.rootUri.fsPath();
    REQUIRE(path.size() >= 2);
    REQUIRE(path[1] == ':');
    auto lowercasedDriveLetter = std::filesystem::path(std::string(1, toupper(path[0])) + path.substr(1));
    auto uppercasedDriveLetter = std::filesystem::path(std::string(1, tolower(path[0])) + path.substr(1));
    CHECK_EQ(workspace.isIgnoredFile(Uri::file((lowercasedDriveLetter / "Packages/_Index/source.luau").generic_string())), true);
    CHECK_EQ(workspace.isIgnoredFile(Uri::file((uppercasedDriveLetter / "Packages/_Index/source.luau").generic_string())), true);
#endif
}

TEST_CASE_FIXTURE(Fixture, "isDefinitionsFile")
{
    client->globalConfig.types.definitionFiles = {{"@roblox", "globalTypes.d.luau"}};

    CHECK_EQ(workspace.isDefinitionFile(workspace.rootUri.resolvePath("source.luau")), false);
    CHECK_EQ(workspace.isDefinitionFile(workspace.rootUri.resolvePath("globalTypes.d.luau")), true);
}

TEST_SUITE_END();
