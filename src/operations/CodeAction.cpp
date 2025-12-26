#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"
#include "Protocol/CodeAction.hpp"
#include "LSP/LuauExt.hpp"
#include "Luau/PrettyPrinter.h"
#include "Luau/LinterConfig.h"
#include "Platform/AutoImports.hpp"

LUAU_FASTFLAG(LuauSolverV2)

namespace
{
// Find a matching diagnostic from the client-provided diagnostics by location
std::optional<lsp::Diagnostic> findMatchingDiagnostic(
    const std::vector<lsp::Diagnostic>& diagnostics, const lsp::Range& range, const std::string& messageSubstring = "")
{
    for (const auto& diag : diagnostics)
    {
        if (diag.range.start.line == range.start.line && diag.range.start.character == range.start.character)
        {
            if (messageSubstring.empty() || diag.message.find(messageSubstring) != std::string::npos)
                return diag;
        }
    }
    return std::nullopt;
}

void generateGlobalUsedAsLocalFix(const lsp::DocumentUri& uri, const Luau::LintWarning& lint, const TextDocument& textDocument,
    const std::optional<lsp::Diagnostic>& diagnostic, std::vector<lsp::CodeAction>& result)
{
    lsp::CodeAction action;
    action.title = "Add 'local' to global variable";
    action.kind = lsp::CodeActionKind::QuickFix;
    action.isPreferred = true;

    if (diagnostic)
        action.diagnostics.push_back(*diagnostic);

    lsp::Position insertPos = textDocument.convertPosition(lint.location.begin);
    lsp::TextEdit edit{{insertPos, insertPos}, "local "};

    lsp::WorkspaceEdit workspaceEdit;
    workspaceEdit.changes.emplace(uri, std::vector{edit});
    action.edit = workspaceEdit;

    result.push_back(action);
}
} // namespace

lsp::WorkspaceEdit WorkspaceFolder::computeOrganiseRequiresEdit(const lsp::DocumentUri& uri)
{
    auto moduleName = fileResolver.getModuleName(uri);
    auto textDocument = fileResolver.getTextDocument(uri);

    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + uri.toString());

    frontend.parse(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return {};

    // Find all the `local X = require(...)` calls
    Luau::LanguageServer::AutoImports::FindImportsVisitor visitor;
    visitor.visit(sourceModule->root);

    // Check if there are any requires
    if (visitor.requiresMap.size() == 1 && visitor.requiresMap.back().empty())
        return {};

    // Treat each require group individually
    std::vector<lsp::TextEdit> edits;
    for (const auto& requireGroup : visitor.requiresMap)
    {
        if (requireGroup.empty())
            continue;

        // Find the first line of the group
        size_t firstRequireLine = requireGroup.begin()->second->location.begin.line;
        Luau::Location previousRequireLocation{{0, 0}, {0, 0}};
        bool isSorted = true;
        for (const auto& [_, stat] : requireGroup)
        {
            if (stat->location.begin < previousRequireLocation.begin)
                isSorted = false;
            previousRequireLocation = stat->location;
            firstRequireLine = stat->location.begin.line < firstRequireLine ? stat->location.begin.line : firstRequireLine;
        }

        // Test to see that if all the requires are already sorted -> if they are, then just leave alone
        // to prevent clogging the undo history stack
        if (isSorted)
            continue;

        // We firstly delete all the previous requires, as they will be added later
        for (const auto& [_, stat] : requireGroup)
            edits.emplace_back(lsp::TextEdit{{{stat->location.begin.line, 0}, {stat->location.begin.line + 1, 0}}, ""});

        // We find the first line to add these services to, and then add them in sorted order
        lsp::Range insertLocation{{firstRequireLine, 0}, {firstRequireLine, 0}};
        for (const auto& [serviceName, stat] : requireGroup)
        {
            // We need to rewrite the statement as we expected it
            auto importText = Luau::toString(stat) + "\n";
            edits.emplace_back(lsp::TextEdit{insertLocation, importText});
        }
    }

    lsp::WorkspaceEdit workspaceEdit;
    workspaceEdit.changes.emplace(uri, edits);
    return workspaceEdit;
}

lsp::CodeActionResult WorkspaceFolder::codeAction(const lsp::CodeActionParams& params, const LSPCancellationToken& cancellationToken)
{
    std::vector<lsp::CodeAction> result;

    auto config = client->getConfiguration(rootUri);
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);

    if (!textDocument)
        return result;

    // Quick fixes from lint warnings
    if (params.context.wants(lsp::CodeActionKind::QuickFix))
    {
        // Run the type checker to get lint warnings
        Luau::CheckResult cr =
            FFlag::LuauSolverV2 ? checkStrict(moduleName, cancellationToken, /* forAutocomplete= */ false) : checkSimple(moduleName, cancellationToken);
        throwIfCancelled(cancellationToken);

        // Convert the requested LSP range to Luau location for filtering
        Luau::Location requestRange = textDocument->convertRange(params.range);

        // Process lint warnings
        for (const auto& lint : cr.lintResult.warnings)
        {
            // Only include lints that overlap with the requested range
            if (!requestRange.overlaps(lint.location))
                continue;

            lsp::Range lintRange = textDocument->convertLocation(lint.location);
            auto diagnostic = findMatchingDiagnostic(params.context.diagnostics, lintRange);

            switch (lint.code)
            {
            case Luau::LintWarning::Code_GlobalUsedAsLocal:
                generateGlobalUsedAsLocalFix(params.textDocument.uri, lint, *textDocument, diagnostic, result);
                break;
            default:
                break;
            }
        }
    }

    // Source actions
    if (params.context.wants(lsp::CodeActionKind::Source) || params.context.wants(lsp::CodeActionKind::SourceOrganizeImports))
    {
        // Add sort requires code action
        lsp::CodeAction organiseImportsAction;
        organiseImportsAction.title = "Sort requires";
        organiseImportsAction.kind = lsp::CodeActionKind::SourceOrganizeImports;

        // TODO: support resolving and defer computation till later
        // if (client->capabilities.textDocument && client->capabilities.textDocument->codeAction &&
        //     client->capabilities.textDocument->codeAction->resolveSupport &&
        //     contains(client->capabilities.textDocument->codeAction->resolveSupport->properties, "edit"))
        organiseImportsAction.edit = computeOrganiseRequiresEdit(params.textDocument.uri);
        result.emplace_back(organiseImportsAction);
    }

    platform->handleCodeAction(params, result);

    return result;
}
