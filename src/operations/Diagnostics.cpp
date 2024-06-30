#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"
#include "LSP/Client.hpp"
#include "LSP/LuauExt.hpp"
#include "LSP/FileUtils.h"

lsp::DocumentDiagnosticReport WorkspaceFolder::documentDiagnostics(const lsp::DocumentDiagnosticParams& params)
{
    client->sendTrace("handling textDocument diagnostics");

    if (!isConfigured)
    {
        lsp::DiagnosticServerCancellationData cancellationData{/*retriggerRequest: */ true};
        throw JsonRpcException(lsp::ErrorCode::ServerCancelled, "server not yet received configuration for diagnostics", cancellationData);
    }

    // TODO: should we apply a resultId and return an unchanged report if unchanged?
    lsp::DocumentDiagnosticReport report;
    std::unordered_map<std::string /* lsp::DocumentUri */, std::vector<lsp::Diagnostic>> relatedDiagnostics{};

    client->sendTrace("[textDocument/diagnostic] retrieving text document");

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        return report; // Bail early with empty report - file was likely closed

    client->sendTrace("[textDocument/diagnostic] running Luau typechecking");

    // Check the module. We do not need to store the type graphs
    Luau::CheckResult cr = checkSimple(moduleName, /* runLintChecks: */ true);

    client->sendTrace("[textDocument/diagnostic] running Luau typechecking COMPLETED");

    // If there was an error retrieving the source module
    // Bail early with an empty report - it is likely that the file was closed
    if (!frontend.getSourceModule(moduleName))
        return report;

    client->sendTrace("[textDocument/diagnostic] retrieving client configuration");

    auto config = client->getConfiguration(rootUri);

    // If the file is a definitions file, then don't display any diagnostics
    if (isDefinitionFile(params.textDocument.uri.fsPath(), config))
        return report;

    client->sendTrace("[textDocument/diagnostic] preparing reports");

    // Report Type Errors
    // Note that type errors can extend to related modules in the require graph - so we report related information here
    for (auto& error : cr.errors)
    {
        if (error.moduleName == moduleName)
        {
            auto diagnostic = createTypeErrorDiagnostic(error, &fileResolver, textDocument);
            report.items.emplace_back(diagnostic);
        }
        else
        {
            auto fileName = platform->resolveToRealPath(error.moduleName);
            if (!fileName || isIgnoredFile(*fileName, config))
                continue;
            auto textDocument = fileResolver.getTextDocumentFromModuleName(error.moduleName);
            auto diagnostic = createTypeErrorDiagnostic(error, &fileResolver, textDocument);
            auto uri = textDocument ? textDocument->uri() : Uri::file(*fileName);
            auto& currentDiagnostics = relatedDiagnostics[uri.toString()];
            currentDiagnostics.emplace_back(diagnostic);
        }
    }

    // Convert the related diagnostics map into an equivalent report
    if (!relatedDiagnostics.empty())
    {
        for (auto& [uri, diagnostics] : relatedDiagnostics)
        {
            // TODO: resultId?
            lsp::SingleDocumentDiagnosticReport subReport{lsp::DocumentDiagnosticReportKind::Full, std::nullopt, diagnostics};
            report.relatedDocuments.emplace(uri, subReport);
        }
    }

    // Report Lint Warnings
    // Lints only apply to the current file
    for (auto& error : cr.lintResult.errors)
    {
        auto diagnostic = createLintDiagnostic(error, textDocument);
        diagnostic.severity = lsp::DiagnosticSeverity::Error; // Report this as an error instead
        report.items.emplace_back(diagnostic);
    }
    for (auto& error : cr.lintResult.warnings)
        report.items.emplace_back(createLintDiagnostic(error, textDocument));

    client->sendTrace("handling textDocument diagnostics COMPLETED");

    return report;
}

lsp::WorkspaceDiagnosticReport WorkspaceFolder::workspaceDiagnostics(const lsp::WorkspaceDiagnosticParams& params)
{
    if (!isConfigured)
    {
        lsp::DiagnosticServerCancellationData cancellationData{/*retriggerRequest: */ true};
        throw JsonRpcException(lsp::ErrorCode::ServerCancelled, "server not yet received configuration for diagnostics", cancellationData);
    }

    lsp::WorkspaceDiagnosticReport workspaceReport;

    // Don't compute any workspace diagnostics for null workspace
    if (isNullWorkspace())
        return workspaceReport;

    auto config = client->getConfiguration(rootUri);

    // Find a list of files to compute diagnostics for
    std::vector<Uri> files{};
    traverseDirectory(this->rootUri.fsPath().generic_string(),
        [&](const std::string& filePath)
        {
            if (isDefinitionFile(filePath, config))
                return;

            auto ext = getExtension(filePath);
            if (ext == ".lua" || ext == ".luau")
                files.push_back(Uri::file(filePath));
        });

    for (auto uri : files)
    {
        auto moduleName = fileResolver.getModuleName(uri);
        auto document = fileResolver.getTextDocument(uri);

        lsp::WorkspaceDocumentDiagnosticReport documentReport;
        documentReport.uri = uri;
        documentReport.kind = lsp::DocumentDiagnosticReportKind::Full;
        if (document)
            documentReport.version = document->version();

        // If we don't have workspace diagnostics enabled, or we are are ignoring this file
        // Then provide an empty report to clear the file diagnostics
        if (!config.diagnostics.workspace || isIgnoredFile(uri, config))
        {
            workspaceReport.items.emplace_back(documentReport);
            continue;
        }

        // Compute new check result
        Luau::CheckResult cr = checkSimple(moduleName, /* runLintChecks: */ true);

        // If there was an error retrieving the source module, disregard this file
        // TODO: should we file a diagnostic?
        if (!frontend.getSourceModule(moduleName))
            continue;

        // Report Type Errors
        // Only report errors for the current file
        for (auto& error : cr.errors)
        {
            if (error.moduleName == moduleName)
            {
                auto diagnostic = createTypeErrorDiagnostic(error, &fileResolver, document);
                documentReport.items.emplace_back(diagnostic);
            }
        }

        // Report Lint Warnings
        for (auto& error : cr.lintResult.errors)
        {
            auto diagnostic = createLintDiagnostic(error, document);
            diagnostic.severity = lsp::DiagnosticSeverity::Error; // Report this as an error instead
            documentReport.items.emplace_back(diagnostic);
        }
        for (auto& error : cr.lintResult.warnings)
            documentReport.items.emplace_back(createLintDiagnostic(error, document));

        workspaceReport.items.emplace_back(documentReport);
    }

    return workspaceReport;
}

lsp::DocumentDiagnosticReport LanguageServer::documentDiagnostic(const lsp::DocumentDiagnosticParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->documentDiagnostics(params);
}

void WorkspaceFolder::pushDiagnostics(const lsp::DocumentUri& uri, const size_t version)
{
    // Convert the diagnostics report into a series of diagnostics published for each relevant file
    lsp::DocumentDiagnosticParams params{lsp::TextDocumentIdentifier{uri}};
    auto diagnostics = documentDiagnostics(params);
    client->publishDiagnostics(lsp::PublishDiagnosticsParams{uri, version, diagnostics.items});

    if (!diagnostics.relatedDocuments.empty())
    {
        for (const auto& [relatedUri, relatedDiagnostics] : diagnostics.relatedDocuments)
        {
            if (relatedDiagnostics.kind == lsp::DocumentDiagnosticReportKind::Full)
            {
                client->publishDiagnostics(lsp::PublishDiagnosticsParams{Uri::parse(relatedUri), std::nullopt, relatedDiagnostics.items});
            }
        }
    }
}

/// Recompute all necessary diagnostics when we detect a configuration (or sourcemap) change
void WorkspaceFolder::recomputeDiagnostics(const ClientConfiguration& config)
{
    // Handle diagnostics if in push-mode
    if ((!client->capabilities.textDocument || !client->capabilities.textDocument->diagnostic))
    {
        // Recompute workspace diagnostics if requested
        if (config.diagnostics.workspace)
        {
            auto diagnostics = workspaceDiagnostics({});
            for (const auto& report : diagnostics.items)
            {
                if (report.kind == lsp::DocumentDiagnosticReportKind::Full)
                {
                    client->publishDiagnostics(lsp::PublishDiagnosticsParams{report.uri, report.version, report.items});
                }
            }
        }
        // Recompute diagnostics for all currently opened files
        else
        {
            for (const auto& [_, document] : fileResolver.managedFiles)
                pushDiagnostics(document.uri(), document.version());
        }
    }
    else
    {
        client->terminateWorkspaceDiagnostics();
        client->refreshWorkspaceDiagnostics();
    }
}

lsp::PartialResponse<lsp::WorkspaceDiagnosticReport> LanguageServer::workspaceDiagnostic(const lsp::WorkspaceDiagnosticParams& params)
{
    lsp::WorkspaceDiagnosticReport fullReport;

    for (auto& workspace : workspaceFolders)
    {
        auto report = workspace->workspaceDiagnostics(params);
        fullReport.items.insert(fullReport.items.end(), std::make_move_iterator(report.items.begin()), std::make_move_iterator(report.items.end()));
    }

    client->workspaceDiagnosticsToken = params.partialResultToken;
    if (params.partialResultToken)
    {
        // Send the initial report as a partial result, and allow streaming of further results
        client->sendProgress({params.partialResultToken.value(), fullReport});
        return std::nullopt;
    }
    else
    {
        return fullReport;
    }
}

void Client::terminateWorkspaceDiagnostics(bool retriggerRequest)
{
    lsp::DiagnosticServerCancellationData cancellationData{retriggerRequest};

    if (this->workspaceDiagnosticsRequestId)
    {
        this->sendError(this->workspaceDiagnosticsRequestId,
            JsonRpcException(lsp::ErrorCode::ServerCancelled, "workspace diagnostics terminated", cancellationData));
    }

    this->workspaceDiagnosticsRequestId = std::nullopt;
    this->workspaceDiagnosticsToken = std::nullopt;
}
