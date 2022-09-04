#include "LSP/Protocol.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"
#include "LSP/Client.hpp"

lsp::DocumentDiagnosticReport WorkspaceFolder::documentDiagnostics(const lsp::DocumentDiagnosticParams& params)
{
    // TODO: should we apply a resultId and return an unchanged report if unchanged?
    lsp::DocumentDiagnosticReport report;
    std::unordered_map<std::string /* lsp::DocumentUri */, std::vector<lsp::Diagnostic>> relatedDiagnostics;

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    Luau::CheckResult cr = frontend.check(moduleName);

    // If there was an error retrieving the source module
    // Bail early with an empty report - it is likely that the file was closed
    if (!frontend.getSourceModule(moduleName))
        return report;

    auto config = client->getConfiguration(rootUri);

    // If the file is a definitions file, then don't display any diagnostics
    if (isDefinitionFile(params.textDocument.uri.fsPath(), config))
        return report;

    // Report Type Errors
    // Note that type errors can extend to related modules in the require graph - so we report related information here
    for (auto& error : cr.errors)
    {
        auto diagnostic = createTypeErrorDiagnostic(error, &fileResolver);
        if (error.moduleName == moduleName)
        {
            report.items.emplace_back(diagnostic);
        }
        else
        {
            auto fileName = fileResolver.resolveToRealPath(error.moduleName);
            if (!fileName || isIgnoredFile(*fileName, config))
                continue;
            auto uri = Uri::file(*fileName);
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
    Luau::LintResult lr = frontend.lint(moduleName);
    for (auto& error : lr.errors)
    {
        auto diagnostic = createLintDiagnostic(error);
        diagnostic.severity = lsp::DiagnosticSeverity::Error; // Report this as an error instead
        report.items.emplace_back(diagnostic);
    }
    for (auto& error : lr.warnings)
        report.items.emplace_back(createLintDiagnostic(error));

    return report;
}

lsp::WorkspaceDiagnosticReport WorkspaceFolder::workspaceDiagnostics(const lsp::WorkspaceDiagnosticParams& params)
{
    lsp::WorkspaceDiagnosticReport workspaceReport;
    auto config = client->getConfiguration(rootUri);

    // Find a list of files to compute diagnostics for
    std::vector<Uri> files;
    for (std::filesystem::recursive_directory_iterator next(this->rootUri.fsPath()), end; next != end; ++next)
    {
        if (next->is_regular_file() && next->path().has_extension() && !isDefinitionFile(next->path(), config))
        {
            auto ext = next->path().extension();
            if (ext == ".lua" || ext == ".luau")
                files.push_back(Uri::file(next->path()));
        }
    }

    for (auto uri : files)
    {
        auto moduleName = fileResolver.getModuleName(uri);
        lsp::WorkspaceDocumentDiagnosticReport documentReport;
        documentReport.uri = uri;
        documentReport.kind = lsp::DocumentDiagnosticReportKind::Full;
        if (fileResolver.isManagedFile(moduleName))
            documentReport.version = fileResolver.managedFiles.at(moduleName).version();

        // If we don't have workspace diagnostics enabled, or we are are ignoring this file
        // Then provide an empty report to clear the file diagnostics
        if (!config.diagnostics.workspace || isIgnoredFile(uri, config))
        {
            workspaceReport.items.emplace_back(documentReport);
            continue;
        }

        // Compute new check result
        Luau::CheckResult cr = frontend.check(moduleName);

        // If there was an error retrieving the source module, disregard this file
        // TODO: should we file a diagnostic?
        if (!frontend.getSourceModule(moduleName))
            continue;

        // Report Type Errors
        // Only report errors for the current file
        for (auto& error : cr.errors)
        {
            auto diagnostic = createTypeErrorDiagnostic(error, &fileResolver);
            if (error.moduleName == moduleName)
            {
                documentReport.items.emplace_back(diagnostic);
            }
        }

        // Report Lint Warnings
        Luau::LintResult lr = frontend.lint(moduleName);
        for (auto& error : lr.errors)
        {
            auto diagnostic = createLintDiagnostic(error);
            diagnostic.severity = lsp::DiagnosticSeverity::Error; // Report this as an error instead
            documentReport.items.emplace_back(diagnostic);
        }
        for (auto& error : lr.warnings)
            documentReport.items.emplace_back(createLintDiagnostic(error));

        workspaceReport.items.emplace_back(documentReport);
    }

    return workspaceReport;
}

lsp::DocumentDiagnosticReport LanguageServer::documentDiagnostic(const lsp::DocumentDiagnosticParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->documentDiagnostics(params);
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