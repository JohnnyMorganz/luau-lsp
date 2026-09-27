#include "doctest.h"
#include "Fixture.h"
#include "Platform/RobloxPlatform.hpp"

TEST_SUITE_BEGIN("Diagnostics");

TEST_CASE_FIXTURE(Fixture, "document_diagnostics_sends_information_for_required_modules")
{
    client->capabilities.textDocument = lsp::TextDocumentClientCapabilities{};
    client->capabilities.textDocument->diagnostic = lsp::DiagnosticClientCapabilities{};
    client->capabilities.textDocument->diagnostic->relatedDocumentSupport = true;

    // Don't show diagnostic for game indexing
    loadDefinition("@extra", "declare game: any");

    registerDocumentForVirtualPath(newDocument("required.luau", R"(
        --!strict
        local x: string = 1
        return {}
    )"),
        "game/Testing/Required");
    auto document = newDocument("main.luau", R"(
        --!strict
        require(game.Testing.Required)
    )");

    auto diagnostics = workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{document}}, nullptr);
    CHECK_EQ(diagnostics.items.size(), 0);
    CHECK_EQ(diagnostics.relatedDocuments.size(), 1);
}

TEST_CASE_FIXTURE(Fixture, "document_diagnostics_does_not_send_information_for_required_modules_if_related_document_support_is_disabled")
{
    client->capabilities.textDocument = lsp::TextDocumentClientCapabilities{};
    client->capabilities.textDocument->diagnostic = lsp::DiagnosticClientCapabilities{};
    client->capabilities.textDocument->diagnostic->relatedDocumentSupport = false;

    // Don't show diagnostic for game indexing
    loadDefinition("@extra", "declare game: any");

    registerDocumentForVirtualPath(newDocument("required.luau", R"(
        --!strict
        local x: string = 1
        return {}
    )"),
        "game/Testing/Required");
    auto document = newDocument("main.luau", R"(
        --!strict
        require(game.Testing.Required)
    )");

    auto diagnostics = workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{document}}, nullptr);
    CHECK_EQ(diagnostics.items.size(), 0);
    CHECK_EQ(diagnostics.relatedDocuments.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "text_document_update_marks_dependent_files_as_dirty")
{
    auto firstDocument = newDocument("a.luau", R"(
        --!strict
        return { hello = true }
    )");
    auto secondDocument = newDocument("b.luau", R"(
        --!strict
        local a = require("./a.luau")
        print(a.hello)
    )");

    auto diagnosticsA = workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{firstDocument}}, nullptr);
    CHECK_EQ(diagnosticsA.items.size(), 0);

    auto diagnosticsB = workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{secondDocument}}, nullptr);
    CHECK_EQ(diagnosticsB.items.size(), 0);

    // We should see diagnostics in the dependent file after the update request
    updateDocument(firstDocument, R"(
        --!strict
        return { hello2 = true }
    )");

    diagnosticsA = workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{firstDocument}}, nullptr);
    CHECK_EQ(diagnosticsA.items.size(), 0);

    diagnosticsB = workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{secondDocument}}, nullptr);
    CHECK_EQ(diagnosticsB.items.size(), 1);
    CHECK_EQ(diagnosticsB.items[0].message, "TypeError: Key 'hello' not found in table '{ hello2: boolean }'");
}

TEST_CASE_FIXTURE(Fixture, "text_document_update_triggers_dependent_diagnostics_in_push_based_diagnostics")
{
    client->globalConfig.diagnostics.includeDependents = true;

    auto firstDocument = newDocument("a.luau", R"(
        --!strict
        return { hello = true }
    )");
    auto secondDocument = newDocument("b.luau", R"(
        --!strict
        local a = require("./a.luau")
        print(a.hello)
    )");

    // Assumption: documents were already checked
    workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{firstDocument}}, nullptr);
    workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{secondDocument}}, nullptr);

    updateDocument(firstDocument, R"(
        --!strict
        return { hello2 = true }
    )");

    REQUIRE(client->notificationQueue.size() > 2);
    auto secondNotification = *client->notificationQueue.rbegin();
    auto firstNotification = *(++client->notificationQueue.rbegin());

    REQUIRE_EQ(firstNotification.first, "textDocument/publishDiagnostics");
    REQUIRE(firstNotification.second);
    lsp::PublishDiagnosticsParams pushedDiagnostics = firstNotification.second.value();
    CHECK_EQ(pushedDiagnostics.uri, firstDocument);
    CHECK_EQ(pushedDiagnostics.diagnostics.size(), 0);

    REQUIRE_EQ(secondNotification.first, "textDocument/publishDiagnostics");
    REQUIRE(secondNotification.second);
    pushedDiagnostics = secondNotification.second.value();
    CHECK_EQ(pushedDiagnostics.uri, secondDocument);
    CHECK_EQ(pushedDiagnostics.diagnostics.size(), 1);
    CHECK_EQ(pushedDiagnostics.diagnostics[0].message, "TypeError: Key 'hello' not found in table '{ hello2: boolean }'");
}

TEST_CASE_FIXTURE(Fixture, "text_document_update_does_not_update_workspace_diagnostics")
{
    client->globalConfig.diagnostics.workspace = true;

    auto firstDocument = newDocument("a.luau", R"(
        --!strict
        return { hello = true }
    )");
    auto secondDocument = newDocument("b.luau", R"(
        --!strict
        local a = require("./a.luau")
        print(a.hello)
    )");

    // Assumption: initial workspace diagnostics was triggered
    // We are using documentDiagnostics to replicate workspace diagnostics checking the file (and making it non-dirty)
    workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{firstDocument}}, nullptr);
    workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{secondDocument}}, nullptr);
    client->workspaceDiagnosticsToken = "WORKSPACE-DIAGNOSTICS-PROGRESS-TOKEN";

    updateDocument(firstDocument, R"(
        --!strict
        return { hello2 = true }
    )");

    // Check no workspace diagnostics progress on queue
    for (const auto& notification : client->notificationQueue)
        CHECK_NE(notification.first, "$/progress");
}

TEST_CASE_FIXTURE(Fixture, "text_document_save_auto_updates_workspace_diagnostics_of_dependent_files")
{
    client->globalConfig.diagnostics.workspace = true;

    auto firstDocument = newDocument("a.luau", R"(
        --!strict
        return { hello = true }
    )");
    auto secondDocument = newDocument("b.luau", R"(
        --!strict
        local a = require("./a.luau")
        print(a.hello)
    )");

    // Assumption: initial workspace diagnostics was triggered
    // We are using documentDiagnostics to replicate workspace diagnostics checking the file (and making it non-dirty)
    workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{firstDocument}}, nullptr);
    workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{secondDocument}}, nullptr);
    client->workspaceDiagnosticsToken = "WORKSPACE-DIAGNOSTICS-PROGRESS-TOKEN";

    updateDocument(firstDocument, R"(
        --!strict
        return { hello2 = true }
    )");
    workspace.onDidSaveTextDocument(firstDocument, lsp::DidSaveTextDocumentParams{{firstDocument}});

    REQUIRE(!client->notificationQueue.empty());
    auto notification = client->notificationQueue.back();
    REQUIRE_EQ(notification.first, "$/progress");
    REQUIRE(notification.second);

    lsp::ProgressParams progressData = notification.second.value();
    REQUIRE_EQ(progressData.token, client->workspaceDiagnosticsToken.value());

    lsp::WorkspaceDiagnosticReportPartialResult diagnostics = progressData.value;
    REQUIRE_EQ(diagnostics.items.size(), 2);

    auto mainDiagnostics = diagnostics.items[0];
    CHECK_EQ(mainDiagnostics.uri, firstDocument);
    CHECK_EQ(mainDiagnostics.items.size(), 0);

    auto dependentDiagnostics = diagnostics.items[1];
    CHECK_EQ(dependentDiagnostics.uri, secondDocument);
    CHECK_EQ(dependentDiagnostics.items.size(), 1);
    CHECK_EQ(dependentDiagnostics.items[0].message, "TypeError: Key 'hello' not found in table '{ hello2: boolean }'");
}

TEST_CASE_FIXTURE(Fixture, "text_document_save_does_not_update_workspace_diagnostics_if_setting_is_disabled")
{
    client->globalConfig.diagnostics.workspace = false;

    auto firstDocument = newDocument("a.luau", R"(
        --!strict
        return { hello = true }
    )");
    auto secondDocument = newDocument("b.luau", R"(
        --!strict
        local a = require("./a.luau")
        print(a.hello)
    )");

    // Assumption: initial workspace diagnostics was triggered
    // We are using documentDiagnostics to replicate workspace diagnostics checking the file (and making it non-dirty)
    workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{firstDocument}}, nullptr);
    workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{secondDocument}}, nullptr);
    client->workspaceDiagnosticsToken = "WORKSPACE-DIAGNOSTICS-PROGRESS-TOKEN";

    updateDocument(firstDocument, R"(
        --!strict
        return { hello2 = true }
    )");
    workspace.onDidSaveTextDocument(firstDocument, lsp::DidSaveTextDocumentParams{{firstDocument}});

    // Check no workspace diagnostics progress on queue
    for (const auto& notification : client->notificationQueue)
        CHECK_NE(notification.first, "$/progress");
}

// Reproduction for https://github.com/JohnnyMorganz/luau-lsp/issues/1019
// "Diagnostics are still reported after closing a file (and workspace diagnostics disabled)"
TEST_CASE_FIXTURE(Fixture, "closing_a_document_clears_diagnostics_when_using_push_diagnostics")
{
    client->globalConfig.diagnostics.workspace = false;

    auto document = newDocument("a.luau", R"(
        --!strict
        local x: string = 1
    )");

    // Simulate the file being open and diagnosed, as would happen on didOpen/didChange
    auto diagnostics = workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{document}}, nullptr);
    CHECK_GT(diagnostics.items.size(), 0);

    client->notificationQueue.clear();
    workspace.closeTextDocument(document);

    REQUIRE(!client->notificationQueue.empty());
    auto notification = client->notificationQueue.back();
    REQUIRE_EQ(notification.first, "textDocument/publishDiagnostics");
    REQUIRE(notification.second);
    lsp::PublishDiagnosticsParams pushedDiagnostics = notification.second.value();
    CHECK_EQ(pushedDiagnostics.uri, document);
    CHECK_EQ(pushedDiagnostics.diagnostics.size(), 0);
}

// Reproduction for https://github.com/JohnnyMorganz/luau-lsp/issues/1019
//
// When the client uses pull diagnostics (`textDocument/diagnostic`) instead of push diagnostics, closing a
// document with workspace diagnostics disabled does not result in any notification or request being sent to
// the client at all - so there is nothing that tells the client to drop the stale diagnostics it already has
// cached for the now-closed document, and they remain visible forever.
//
// This matches the maintainer's follow-up on the issue: the server has no direct way to clear diagnostics for
// a single closed document under the pull model (only a client capability-gated `workspace/diagnostic/refresh`
// request, which asks the client to re-pull diagnostics for its currently open documents - not the one that
// was just closed), so this is ultimately something the client is responsible for handling.
TEST_CASE_FIXTURE(Fixture, "closing_a_document_does_not_clear_diagnostics_when_using_pull_diagnostics_without_refresh_support")
{
    client->capabilities.textDocument = lsp::TextDocumentClientCapabilities{};
    client->capabilities.textDocument->diagnostic = lsp::DiagnosticClientCapabilities{};
    client->globalConfig.diagnostics.workspace = false;

    auto document = newDocument("a.luau", R"(
        --!strict
        local x: string = 1
    )");

    // Simulate the file being open and diagnosed via a pull request, as would happen from the client
    auto diagnostics = workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{document}}, nullptr);
    CHECK_GT(diagnostics.items.size(), 0);

    client->notificationQueue.clear();
    client->requestQueue.clear();
    workspace.closeTextDocument(document);

    // Nothing was sent to the client to indicate that the diagnostics for the closed document should be
    // cleared - reproducing the bug where stale diagnostics linger in the Problems panel after close
    CHECK(client->notificationQueue.empty());
    CHECK(client->requestQueue.empty());
}

// Reproduction for https://github.com/JohnnyMorganz/luau-lsp/issues/1019
//
// Even when the client declares `workspace.diagnostics.refreshSupport`, the server can only ask it to refresh
// its diagnostics in general (`workspace/diagnostic/refresh`) - it cannot target the specific document that was
// just closed. Per the pull diagnostics model, it is then up to the client to realise the document is closed
// and drop its cached diagnostics for it, rather than re-requesting them.
TEST_CASE_FIXTURE(Fixture, "closing_a_document_only_triggers_a_generic_refresh_request_when_using_pull_diagnostics_with_refresh_support")
{
    client->capabilities.textDocument = lsp::TextDocumentClientCapabilities{};
    client->capabilities.textDocument->diagnostic = lsp::DiagnosticClientCapabilities{};
    client->capabilities.workspace = lsp::ClientWorkspaceCapabilities{};
    client->capabilities.workspace->diagnostics = lsp::DiagnosticWorkspaceClientCapabilities{};
    client->capabilities.workspace->diagnostics->refreshSupport = true;
    client->globalConfig.diagnostics.workspace = false;

    auto document = newDocument("a.luau", R"(
        --!strict
        local x: string = 1
    )");

    auto diagnostics = workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{document}}, nullptr);
    CHECK_GT(diagnostics.items.size(), 0);

    client->notificationQueue.clear();
    client->requestQueue.clear();
    workspace.closeTextDocument(document);

    // No notification is sent, only a generic (non-document-specific) refresh request
    CHECK(client->notificationQueue.empty());
    REQUIRE(!client->requestQueue.empty());
    auto request = client->requestQueue.back();
    CHECK_EQ(request.first, "workspace/diagnostic/refresh");
}

TEST_CASE_FIXTURE(Fixture, "document_diagnostics_respects_cancellation")
{
    auto cancellationToken = std::make_shared<Luau::FrontendCancellationToken>();
    cancellationToken->cancel();

    auto document = newDocument("a.luau", "local x = 1");
    CHECK_THROWS_AS(workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{document}}, cancellationToken), RequestCancelledException);
}

TEST_SUITE_END();
