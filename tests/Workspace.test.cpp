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

    Uri uri = Uri::file(workspace.rootUri.fsPath() / "source.luau");
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

    CHECK_EQ(workspace.isIgnoredFile(workspace.rootUri.fsPath() / "source.luau"), false);
    CHECK_EQ(workspace.isIgnoredFile(workspace.rootUri.fsPath() / "Packages/_Index/source.luau"), true);

#ifdef _WIN32
    CHECK_EQ(workspace.isIgnoredFile("c:/Users/random/folder/Packages/_Index/source.luau"), true);
    CHECK_EQ(workspace.isIgnoredFile("C:/Users/random/folder/Packages/_Index/source.luau"), true);
#endif
}

TEST_SUITE_END();
