#include "doctest.h"
#include "LSP/LanguageServer.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "Protocol/Lifecycle.hpp"
#include "TestClient.h"

TEST_SUITE_BEGIN("LanguageServer");

LUAU_FASTFLAG(DebugLuauTimeTracing)

TEST_CASE("language_server_handles_fflags_in_initialization_options")
{
    TestClient client;
    LanguageServer server(&client, std::nullopt);

    InitializationOptions initializationOptions{};
    initializationOptions.fflags.insert_or_assign("DebugLuauTimeTracing", "True");

    lsp::InitializeParams params;
    params.initializationOptions = initializationOptions;
    server.onRequest(0, "initialize", params);

    CHECK_EQ(FFlag::DebugLuauTimeTracing.value, true);

    server.shutdown();

    // NOTE: Setting FFlags can virally affect other tests! We must reset here
    FFlag::DebugLuauTimeTracing.value = false;
}

TEST_CASE("language_server_lazily_initializes_workspace_folders")
{
    TestClient client;
    LanguageServer server(&client, std::nullopt);

    // Indexing throws errors as the workspace doesn't exist
    client.globalConfig.index.enabled = false;

    auto workspaceUri = Uri::file("project");

    lsp::InitializeParams initializeParams;
    std::vector<lsp::WorkspaceFolder> workspaceFolders;
    workspaceFolders.emplace_back(lsp::WorkspaceFolder{workspaceUri, "project"});
    initializeParams.workspaceFolders = workspaceFolders;

    server.onRequest(0, "initialize", initializeParams);
    server.onNotification("initialized", std::make_optional(lsp::InitializedParams{}));

    auto uri = workspaceUri.resolvePath("example.luau");
    auto workspaceFolder = server.findWorkspace(uri, /* shouldInitialize= */ false);

    REQUIRE(workspaceFolder);
    CHECK_FALSE(workspaceFolder->isNullWorkspace());
    CHECK_FALSE(workspaceFolder->isReady);

    lsp::DidOpenTextDocumentParams openParams;
    openParams.textDocument = {uri, "luau", 0, "print()"};
    server.onNotification("textDocument/didOpen", std::make_optional(openParams));

    CHECK(workspaceFolder->isReady);

    server.shutdown();
}

TEST_CASE("language_server_initializes_workspace_folders_for_studio_plugin_messages")
{
    TestClient client;
    LanguageServer server(&client, std::nullopt);

    // Indexing throws errors as the workspace doesn't exist
    client.globalConfig.index.enabled = false;

    auto workspaceUri = Uri::file("project");

    lsp::InitializeParams initializeParams;
    std::vector<lsp::WorkspaceFolder> workspaceFolders;
    workspaceFolders.emplace_back(lsp::WorkspaceFolder{workspaceUri, "project"});
    initializeParams.workspaceFolders = workspaceFolders;

    server.onRequest(0, "initialize", initializeParams);
    server.onNotification("initialized", std::make_optional(lsp::InitializedParams{}));

    auto workspaceFolder = server.findWorkspace(workspaceUri.resolvePath("example.luau"), /* shouldInitialize= */ false);
    REQUIRE(workspaceFolder);
    CHECK_FALSE(workspaceFolder->isReady);

    // The Studio plugin can send the DataModel before any file in the workspace is opened
    server.onNotification("$/plugin/full", json::parse(R"({
        "Id": 1, "Session": "session-1", "Name": "game", "ClassName": "DataModel", "Children": [
            { "Id": 2, "Name": "ReplicatedStorage", "ClassName": "ReplicatedStorage", "Children": [
                { "Id": 3, "Name": "Assets", "ClassName": "Folder" }
            ] }
        ]
    })"));

    CHECK(workspaceFolder->isReady);
    auto platform = dynamic_cast<RobloxPlatform*>(workspaceFolder->platform.get());
    REQUIRE(platform);
    CHECK(platform->rootSourceNode->walkPath("ReplicatedStorage/Assets"));

    server.shutdown();
}

TEST_CASE("language_server_can_process_string_ids")
{
    TestClient client;
    LanguageServer server(&client, std::nullopt);

    auto workspaceUri = Uri::file("project");
    lsp::InitializeParams initializeParams;
    std::vector<lsp::WorkspaceFolder> workspaceFolders;
    workspaceFolders.emplace_back(lsp::WorkspaceFolder{workspaceUri, "project"});
    initializeParams.workspaceFolders = workspaceFolders;

    server.handleMessage(json_rpc::JsonRpcMessage{"0", "initialize", initializeParams});
    server.shutdown();

    REQUIRE(client.errorQueue.empty());
}

TEST_SUITE_END();
