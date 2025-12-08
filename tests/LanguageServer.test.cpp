#include "doctest.h"
#include "LSP/LanguageServer.hpp"
#include "LSP/JsonRpc.hpp"
#include "Protocol/Lifecycle.hpp"
#include "TestClient.h"
#include "TempDir.h"

TEST_SUITE_BEGIN("LanguageServer");

LUAU_FASTFLAG(DebugLuauTimeTracing)

TEST_CASE("language_server_handles_fflags_in_initialization_options")
{
    Client client;
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
    Client client;
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

TEST_CASE("language_server_routes_platform_specific_requests")
{
    TempDir t("lsp_platform_request");
    TestClient client;
    client.globalConfig.index.enabled = false;
    LanguageServer server(&client, std::nullopt);

    auto workspaceUri = Uri::file(t.path());
    lsp::InitializeParams initializeParams;
    std::vector<lsp::WorkspaceFolder> workspaceFolders;
    workspaceFolders.emplace_back(lsp::WorkspaceFolder{workspaceUri, "project"});
    initializeParams.workspaceFolders = workspaceFolders;

    server.onRequest(0, "initialize", initializeParams);
    server.onNotification("initialized", std::make_optional(lsp::InitializedParams{}));

    // Force workspace to be ready
    lsp::DidOpenTextDocumentParams openParams;
    openParams.textDocument = {workspaceUri.resolvePath("init.luau"), "luau", 0, "print()"};
    server.onNotification("textDocument/didOpen", std::make_optional(openParams));

    // $/plugin/getFilePaths should be routed to RobloxPlatform
    server.onRequest(1, "$/plugin/getFilePaths", std::nullopt);

    server.shutdown();

    // Should not have any errors - the request was handled
    CHECK(client.errorQueue.empty());
}

TEST_CASE("language_server_throws_method_not_found_for_unknown_methods")
{
    TempDir t("lsp_unknown_method");
    TestClient client;
    client.globalConfig.index.enabled = false;
    LanguageServer server(&client, std::nullopt);

    auto workspaceUri = Uri::file(t.path());
    lsp::InitializeParams initializeParams;
    std::vector<lsp::WorkspaceFolder> workspaceFolders;
    workspaceFolders.emplace_back(lsp::WorkspaceFolder{workspaceUri, "project"});
    initializeParams.workspaceFolders = workspaceFolders;

    server.onRequest(0, "initialize", initializeParams);
    server.onNotification("initialized", std::make_optional(lsp::InitializedParams{}));

    // Unknown method should throw
    CHECK_THROWS_AS(server.onRequest(1, "$/unknown/nonexistent", std::nullopt), JsonRpcException);

    server.shutdown();
}

TEST_SUITE_END();
