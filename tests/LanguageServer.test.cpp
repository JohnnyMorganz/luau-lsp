#include "doctest.h"
#include "LSP/LanguageServer.hpp"
#include "Protocol/Lifecycle.hpp"
#include "TestClient.h"

#include <algorithm>

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

// Reproduction/fix for https://github.com/JohnnyMorganz/luau-lsp/issues/1019
//
// The server used to always advertise `diagnosticProvider.workspaceDiagnostics: true` statically,
// regardless of the (dynamic, resource-scoped) `diagnostics.workspace` setting. Pull-diagnostics
// clients use that capability to decide whether closing a document can be cleared immediately, or
// whether to defer to a workspace diagnostic pull - so misdeclaring it as `true` when workspace
// diagnostics is actually disabled left closed documents' diagnostics stale forever. If the client
// supports dynamic registration for diagnostics, the server now registers the real capability once
// configuration is known (and keeps it in sync as configuration changes), instead of statically
// declaring it at initialize.
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

    auto it = std::find_if(client.requestQueue.begin(), client.requestQueue.end(),
        [](const auto& request)
        {
            return request.first == "client/registerCapability";
        });
    REQUIRE(it != client.requestQueue.end());
    REQUIRE(it->second);
    lsp::RegistrationParams registrationParams = it->second.value();
    REQUIRE_EQ(registrationParams.registrations.size(), 1);
    CHECK_EQ(registrationParams.registrations[0].method, "textDocument/diagnostic");
    lsp::DiagnosticOptions registerOptions = registrationParams.registrations[0].registerOptions;
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

    auto it = std::find_if(client.requestQueue.begin(), client.requestQueue.end(),
        [](const auto& request)
        {
            if (request.first != "client/registerCapability" || !request.second)
                return false;
            lsp::RegistrationParams registrationParams = request.second.value();
            return !registrationParams.registrations.empty() && registrationParams.registrations[0].method == "textDocument/diagnostic";
        });
    CHECK(it == client.requestQueue.end());

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

    auto unregisterIt = std::find_if(client.requestQueue.begin(), client.requestQueue.end(),
        [](const auto& request)
        {
            return request.first == "client/unregisterCapability";
        });
    REQUIRE(unregisterIt != client.requestQueue.end());

    auto registerIt = std::find_if(client.requestQueue.begin(), client.requestQueue.end(),
        [](const auto& request)
        {
            return request.first == "client/registerCapability";
        });
    REQUIRE(registerIt != client.requestQueue.end());
    REQUIRE(registerIt->second);
    lsp::RegistrationParams registrationParams = registerIt->second.value();
    REQUIRE_EQ(registrationParams.registrations.size(), 1);
    lsp::DiagnosticOptions registerOptions = registrationParams.registrations[0].registerOptions;
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
