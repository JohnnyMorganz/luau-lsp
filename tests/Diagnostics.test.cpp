#include "doctest.h"
#include "Fixture.h"
#include "TempDir.h"
#include "Platform/RobloxPlatform.hpp"

TEST_SUITE_BEGIN("Diagnostics");

TEST_CASE_FIXTURE(Fixture, "document_diagnostics_sends_information_for_required_modules")
{
    client->capabilities.textDocument = lsp::TextDocumentClientCapabilities{};
    client->capabilities.textDocument->diagnostic = lsp::DiagnosticClientCapabilities{};
    client->capabilities.textDocument->diagnostic->relatedDocumentSupport = true;

    // Don't show diagnostic for game indexing
    loadDefinition("declare game: any");

    registerDocumentForVirtualPath(newDocument("required.luau", R"(
        --!strict
        local x: string = 1
        return {}
    )"), "game/Testing/Required");
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
    loadDefinition("declare game: any");

    registerDocumentForVirtualPath(newDocument("required.luau", R"(
        --!strict
        local x: string = 1
        return {}
    )"), "game/Testing/Required");
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

TEST_CASE_FIXTURE(Fixture, "document_diagnostics_respects_cancellation")
{
    auto cancellationToken = std::make_shared<Luau::FrontendCancellationToken>();
    cancellationToken->cancel();

    auto document = newDocument("a.luau", "local x = 1");
    CHECK_THROWS_AS(workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{document}}, cancellationToken), RequestCancelledException);
}

TEST_SUITE_END();
