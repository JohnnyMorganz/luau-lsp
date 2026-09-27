#include "doctest.h"
#include "LSP/LanguageServer.hpp"
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

// #1019: a pull-diagnostics client uses `diagnosticProvider.workspaceDiagnostics` to decide whether
// to clear a closed document's diagnostics immediately or defer to a workspace pull, so this must
// reflect the real `diagnostics.workspace` setting rather than always being statically `true`.
TEST_CASE("language_server_dynamically_registers_diagnostics_capability_when_client_supports_it")
{
    TestClient client;
    LanguageServer server(&client, std::nullopt);
    client.globalConfig.diagnostics.workspace = false;

    lsp::InitializeParams params;
    params.capabilities.textDocument = lsp::TextDocumentClientCapabilities{};
    params.capabilities.textDocument->diagnostic = lsp::DiagnosticClientCapabilities{};
    params.capabilities.textDocument->diagnostic->dynamicRegistration = true;

    server.onRequest(0, "initialize", params);
    server.onNotification("initialized", std::make_optional(lsp::InitializedParams{}));

    auto registration = client.findRegistration("textDocument/diagnostic");
    REQUIRE(registration);
    lsp::DiagnosticOptions registerOptions = registration->registerOptions;
    CHECK_FALSE(registerOptions.workspaceDiagnostics);

    server.shutdown();
}

TEST_CASE("language_server_does_not_dynamically_register_diagnostics_capability_without_client_support")
{
    TestClient client;
    LanguageServer server(&client, std::nullopt);

    // No `textDocument.diagnostic.dynamicRegistration` capability declared
    lsp::InitializeParams params;
    server.onRequest(0, "initialize", params);
    server.onNotification("initialized", std::make_optional(lsp::InitializedParams{}));

    CHECK_FALSE(client.findRegistration("textDocument/diagnostic"));

    server.shutdown();
}

TEST_CASE("language_server_updates_diagnostics_capability_registration_when_workspace_diagnostics_setting_changes")
{
    TestClient client;
    LanguageServer server(&client, std::nullopt);
    client.globalConfig.diagnostics.workspace = false;

    lsp::InitializeParams params;
    params.capabilities.textDocument = lsp::TextDocumentClientCapabilities{};
    params.capabilities.textDocument->diagnostic = lsp::DiagnosticClientCapabilities{};
    params.capabilities.textDocument->diagnostic->dynamicRegistration = true;

    server.onRequest(0, "initialize", params);
    server.onNotification("initialized", std::make_optional(lsp::InitializedParams{}));

    // Make the null workspace ready so `configChangedCallback` doesn't bail out early
    lsp::DidOpenTextDocumentParams openParams;
    openParams.textDocument = {Uri::file("untitled.luau"), "luau", 0, "print()"};
    server.onNotification("textDocument/didOpen", std::make_optional(openParams));

    client.requestQueue.clear();

    ClientConfiguration oldConfig;
    oldConfig.diagnostics.workspace = false;
    ClientConfiguration newConfig;
    newConfig.diagnostics.workspace = true;
    // Mirror what LSPClient::requestConfiguration's response handler does: store the new
    // configuration before invoking the callback, since `updateDiagnosticCapabilityRegistration`
    // reads the current configuration back via `client->getConfiguration`.
    client.configStore.insert_or_assign(Uri(), newConfig);
    client.configChangedCallback(Uri(), newConfig, &oldConfig);

    CHECK(client.hasUnregistration("textDocument/diagnostic"));

    auto registration = client.findRegistration("textDocument/diagnostic");
    REQUIRE(registration);
    lsp::DiagnosticOptions registerOptions = registration->registerOptions;
    CHECK(registerOptions.workspaceDiagnostics);

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
