#include "doctest.h"
#include "LSP/LanguageServer.hpp"
#include "Protocol/Lifecycle.hpp"

TEST_SUITE_BEGIN("LanguageServer");

LUAU_FASTFLAG(DebugLuauTimeTracing);

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

    server.onRequest(1, "shutdown", {});

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

    server.onRequest(1, "shutdown", {});
}

TEST_SUITE_END();
