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

    auto diagnostics = workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{document}});
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

    auto diagnostics = workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{document}});
    CHECK_EQ(diagnostics.items.size(), 0);
    CHECK_EQ(diagnostics.relatedDocuments.size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "text_document_update_triggers_diagnostics_in_dependent_file")
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

    auto diagnosticsA = workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{firstDocument}});
    CHECK_EQ(diagnosticsA.items.size(), 0);

    auto diagnosticsB = workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{secondDocument}});
    CHECK_EQ(diagnosticsB.items.size(), 0);

    updateDocument(firstDocument, R"(
        --!strict
        return { hello2 = true }
    )");

    diagnosticsA = workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{firstDocument}});
    CHECK_EQ(diagnosticsA.items.size(), 0);

    diagnosticsB = workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{secondDocument}});
    CHECK_EQ(diagnosticsB.items.size(), 1);
    if (FFlag::LuauSolverV2)
        CHECK_EQ(diagnosticsB.items[0].message, "TypeError: Key 'hello' not found in table '{ hello2: boolean }'");
    else
        CHECK_EQ(diagnosticsB.items[0].message, "TypeError: Key 'hello' not found in table '{| hello2: boolean |}'");
}

TEST_CASE_FIXTURE(Fixture, "text_document_update_auto_updates_workspace_diagnostics_of_dependent_files")
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

    // Assumption: initial workspace diagnostics was triggered
    // We are using documentDiagnostics to replicate workspace diagnostics checking the file (and making it non-dirty)
    workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{firstDocument}});
    workspace.documentDiagnostics(lsp::DocumentDiagnosticParams{{secondDocument}});
    client->workspaceDiagnosticsToken = "WORKSPACE-DIAGNOSTICS-PROGRESS-TOKEN";

    updateDocument(firstDocument, R"(
        --!strict
        return { hello2 = true }
    )");

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
    if (FFlag::LuauSolverV2)
        CHECK_EQ(dependentDiagnostics.items[0].message, "TypeError: Key 'hello' not found in table '{ hello2: boolean }'");
    else
        CHECK_EQ(dependentDiagnostics.items[0].message, "TypeError: Key 'hello' not found in table '{| hello2: boolean |}'");
}

TEST_SUITE_END();
