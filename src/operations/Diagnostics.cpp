#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"
#include "LSP/Client.hpp"
#include "LSP/LuauExt.hpp"
#include "Luau/TimeTrace.h"

static bool supportsRelatedDocuments(const lsp::ClientCapabilities& capabilities)
{
    return capabilities.textDocument && capabilities.textDocument->diagnostic && capabilities.textDocument->diagnostic->relatedDocumentSupport;
}

lsp::DocumentDiagnosticReport WorkspaceFolder::documentDiagnostics(const lsp::DocumentDiagnosticParams& params)
{
    LUAU_TIMETRACE_SCOPE("WorkspaceFolder::documentDiagnostics", "LSP");
    if (!isConfigured)
    {
        lsp::DiagnosticServerCancellationData cancellationData{/*retriggerRequest: */ true};
        throw JsonRpcException(lsp::ErrorCode::ServerCancelled, "server not yet received configuration for diagnostics", cancellationData);
    }

    // TODO: should we apply a resultId and return an unchanged report if unchanged?
    lsp::DocumentDiagnosticReport report;
    std::unordered_map<Uri, std::vector<lsp::Diagnostic>, UriHash> relatedDiagnostics{};

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        return report; // Bail early with empty report - file was likely closed

    // Check the module
    // In the new solver, we end up calling `checkStrict` (retain type graphs), because documentation diagnostics is typically
    // on the file a user is working on. So, we will end up having to call checkStrict later for Hover etc. i.e., calling 2 typechecks
    // for no reason.
    // In the old solver, it doesn't really matter, because there is a differnce between module + moduleForAutocomplete. So we prefer
    // using checkSimple as we won't use the type graphs
    Luau::CheckResult cr = FFlag::LuauSolverV2 ? checkStrict(moduleName) : checkSimple(moduleName);

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
        if (error.moduleName == moduleName)
        {
            auto diagnostic = createTypeErrorDiagnostic(error, &fileResolver, textDocument);
            report.items.emplace_back(diagnostic);
        }
        else if (supportsRelatedDocuments(client->capabilities))
        {
            auto fileName = platform->resolveToRealPath(error.moduleName);
            if (!fileName || isIgnoredFile(*fileName, config))
                continue;
            auto textDocument = fileResolver.getTextDocumentFromModuleName(error.moduleName);
            auto diagnostic = createTypeErrorDiagnostic(error, &fileResolver, textDocument);
            auto uri = textDocument ? textDocument->uri() : Uri::file(*fileName);
            auto& currentDiagnostics = relatedDiagnostics[uri];
            currentDiagnostics.emplace_back(diagnostic);
        }
    }

    // Convert the related diagnostics map into an equivalent report
    if (supportsRelatedDocuments(client->capabilities) && !relatedDiagnostics.empty())
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

    return report;
}

std::vector<Uri> WorkspaceFolder::findFilesForWorkspaceDiagnostics(const std::filesystem::path& rootPath, const ClientConfiguration& config)
{
    LUAU_TIMETRACE_SCOPE("WorkspaceFolder::findFilesForWorkspaceDiagnostics", "LSP");

    std::vector<Uri> files{};
    for (std::filesystem::recursive_directory_iterator next(rootPath, std::filesystem::directory_options::skip_permission_denied), end; next != end;
         ++next)
    {
        try
        {
            if (next->is_regular_file() && next->path().has_extension() && !isDefinitionFile(next->path(), config))
            {
                auto ext = next->path().extension();
                if (ext == ".lua" || ext == ".luau")
                    files.push_back(Uri::file(next->path()));
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            client->sendLogMessage(lsp::MessageType::Warning, std::string("failed to compute workspace diagnostics for file: ") + e.what());
        }
    }
    return files;
}

lsp::WorkspaceDiagnosticReport WorkspaceFolder::workspaceDiagnostics(const lsp::WorkspaceDiagnosticParams& params)
{
    LUAU_TIMETRACE_SCOPE("WorkspaceFolder::workspaceDiagnostics", "LSP");
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
    auto files = findFilesForWorkspaceDiagnostics(rootUri.fsPath(), config);
    workspaceReport.items.reserve(files.size());

    for (auto uri : files)
    {
        lsp::WorkspaceDocumentDiagnosticReport documentReport;
        documentReport.uri = uri;
        documentReport.kind = lsp::DocumentDiagnosticReportKind::Full;

        // If we don't have workspace diagnostics enabled, or we are are ignoring this file
        // Then provide an empty report to clear the file diagnostics
        if (!config.diagnostics.workspace || isIgnoredFile(uri, config))
        {
            workspaceReport.items.emplace_back(documentReport);
            continue;
        }

        auto moduleName = fileResolver.getModuleName(uri);
        auto document = fileResolver.getTextDocument(uri);
        if (document)
            documentReport.version = document->version();

        // Compute new check result
        Luau::CheckResult cr = checkSimple(moduleName);

        // If there was an error retrieving the source module, disregard this file
        // TODO: should we file a diagnostic?
        if (!frontend.getSourceModule(moduleName))
            continue;

        documentReport.items.reserve(cr.errors.size() + cr.lintResult.errors.size() + cr.lintResult.warnings.size());

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

    try
    {
        auto diagnostics = documentDiagnostics(params);
        client->publishDiagnostics(lsp::PublishDiagnosticsParams{uri, version, diagnostics.items});
        if (!diagnostics.relatedDocuments.empty())
        {
            for (const auto& [relatedUri, relatedDiagnostics] : diagnostics.relatedDocuments)
            {
                if (relatedDiagnostics.kind == lsp::DocumentDiagnosticReportKind::Full)
                {
                    client->publishDiagnostics(lsp::PublishDiagnosticsParams{relatedUri, std::nullopt, relatedDiagnostics.items});
                }
            }
        }
    }
    catch (const JsonRpcException&)
    {
        // Server is not yet configured to send diagnostic messages
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
