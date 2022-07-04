#include "LSP/Protocol.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"

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

    // If the file is ignored, and is *not* loaded in, then don't display any diagnostics
    if (isIgnoredFile(params.textDocument.uri.fsPath()) && !fileResolver.isManagedFile(moduleName))
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

    // If we don't have workspace diagnostics enabled, then just return an empty report
    if (!config.diagnostics.workspace)
        return workspaceReport;

    // TODO: we should handle non-sourcemap features
    std::vector<SourceNodePtr> queue;
    if (fileResolver.rootSourceNode)
    {
        queue.push_back(fileResolver.rootSourceNode);
    };

    while (!queue.empty())
    {
        auto node = queue.back();
        queue.pop_back();
        for (auto& child : node->children)
        {
            queue.push_back(child);
        }

        auto realPath = fileResolver.getRealPathFromSourceNode(node);
        auto moduleName = fileResolver.getVirtualPathFromSourceNode(node);

        if (!realPath || isIgnoredFile(*realPath, config))
            continue;

        // Compute new check result
        Luau::CheckResult cr = frontend.check(moduleName);

        // If there was an error retrieving the source module, disregard this file
        // TODO: should we file a diagnostic?
        if (!frontend.getSourceModule(moduleName))
            continue;

        lsp::WorkspaceDocumentDiagnosticReport documentReport;
        documentReport.uri = Uri::file(*realPath);
        documentReport.kind = lsp::DocumentDiagnosticReportKind::Full;
        if (fileResolver.isManagedFile(moduleName))
        {
            documentReport.version = fileResolver.managedFiles.at(moduleName).version();
        }

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

lsp::WorkspaceDiagnosticReport LanguageServer::workspaceDiagnostic(const lsp::WorkspaceDiagnosticParams& params)
{
    lsp::WorkspaceDiagnosticReport fullReport;

    for (auto& workspace : workspaceFolders)
    {
        auto report = workspace->workspaceDiagnostics(params);
        fullReport.items.insert(fullReport.items.end(), std::make_move_iterator(report.items.begin()), std::make_move_iterator(report.items.end()));
    }

    return fullReport;
}