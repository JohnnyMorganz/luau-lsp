#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"
#include "Protocol/CodeAction.hpp"
#include "LSP/LuauExt.hpp"
#include "Luau/PrettyPrinter.h"
#include "Luau/LinterConfig.h"
#include "Platform/AutoImports.hpp"
#include "Luau/Ast.h"

#include <set>

LUAU_FASTFLAG(LuauSolverV2)

namespace
{
// Visitor to find the statement containing a local variable at a specific location
struct FindStatementContainingLocal : public Luau::AstVisitor
{
    Luau::Location targetLocation;
    Luau::AstStat* result = nullptr;

    explicit FindStatementContainingLocal(const Luau::Location& location)
        : targetLocation(location)
    {
    }

    bool visit(Luau::AstStatLocal* node) override
    {
        for (size_t i = 0; i < node->vars.size; ++i)
        {
            if (node->vars.data[i]->location == targetLocation)
            {
                result = node;
                return false;
            }
        }
        return true;
    }

    bool visit(Luau::AstStatLocalFunction* node) override
    {
        if (node->name->location == targetLocation)
        {
            result = node;
            return false;
        }
        return true;
    }
};

Luau::AstStat* findStatementContainingLocal(Luau::AstStatBlock* root, const Luau::Location& location)
{
    FindStatementContainingLocal finder(location);
    root->visit(&finder);
    return finder.result;
}

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

void generateUnusedVariableFixes(const lsp::DocumentUri& uri, const Luau::LintWarning& lint, const TextDocument& textDocument,
    Luau::AstStatBlock* root, const std::optional<lsp::Diagnostic>& diagnostic, std::vector<lsp::CodeAction>& result)
{
    // Get the variable name from the lint location
    lsp::Range lintRange = textDocument.convertLocation(lint.location);
    std::string varName = textDocument.getText(lintRange);

    // Fix 1: Prefix with '_' to silence the lint
    {
        lsp::CodeAction action;
        action.title = "Prefix '" + varName + "' with '_' to silence";
        action.kind = lsp::CodeActionKind::QuickFix;
        action.isPreferred = false;

        if (diagnostic)
            action.diagnostics.push_back(*diagnostic);

        lsp::Position insertPos = textDocument.convertPosition(lint.location.begin);
        lsp::TextEdit edit{{insertPos, insertPos}, "_"};

        lsp::WorkspaceEdit workspaceEdit;
        workspaceEdit.changes.emplace(uri, std::vector{edit});
        action.edit = workspaceEdit;

        result.push_back(action);
    }

    // Fix 2: Delete the variable declaration
    {
        auto statement = findStatementContainingLocal(root, lint.location);

        if (statement)
        {
            lsp::CodeAction action;
            action.title = "Remove unused variable: '" + varName + "'";
            action.kind = lsp::CodeActionKind::QuickFix;
            action.isPreferred = false;

            if (diagnostic)
                action.diagnostics.push_back(*diagnostic);

            // Delete the entire line containing the statement
            lsp::Range deleteRange{
                {statement->location.begin.line, 0},
                {statement->location.end.line + 1, 0}
            };
            lsp::TextEdit edit{deleteRange, ""};

            lsp::WorkspaceEdit workspaceEdit;
            workspaceEdit.changes.emplace(uri, std::vector{edit});
            action.edit = workspaceEdit;

            result.push_back(action);
        }
    }
}

void generateUnusedFunctionFixes(const lsp::DocumentUri& uri, const Luau::LintWarning& lint, const TextDocument& textDocument,
    Luau::AstStatBlock* root, const std::optional<lsp::Diagnostic>& diagnostic, std::vector<lsp::CodeAction>& result)
{
    // Get the function name from the lint location
    lsp::Range lintRange = textDocument.convertLocation(lint.location);
    std::string funcName = textDocument.getText(lintRange);

    // Fix 1: Prefix with '_' to silence the lint
    {
        lsp::CodeAction action;
        action.title = "Prefix '" + funcName + "' with '_' to silence";
        action.kind = lsp::CodeActionKind::QuickFix;
        action.isPreferred = false;

        if (diagnostic)
            action.diagnostics.push_back(*diagnostic);

        lsp::Position insertPos = textDocument.convertPosition(lint.location.begin);
        lsp::TextEdit edit{{insertPos, insertPos}, "_"};

        lsp::WorkspaceEdit workspaceEdit;
        workspaceEdit.changes.emplace(uri, std::vector{edit});
        action.edit = workspaceEdit;

        result.push_back(action);
    }

    // Fix 2: Delete the function declaration
    {
        auto statement = findStatementContainingLocal(root, lint.location);

        if (statement)
        {
            lsp::CodeAction action;
            action.title = "Remove unused function: '" + funcName + "'";
            action.kind = lsp::CodeActionKind::QuickFix;
            action.isPreferred = false;

            if (diagnostic)
                action.diagnostics.push_back(*diagnostic);

            // Delete the entire function
            lsp::Range deleteRange{
                {statement->location.begin.line, 0},
                {statement->location.end.line + 1, 0}
            };
            lsp::TextEdit edit{deleteRange, ""};

            lsp::WorkspaceEdit workspaceEdit;
            workspaceEdit.changes.emplace(uri, std::vector{edit});
            action.edit = workspaceEdit;

            result.push_back(action);
        }
    }
}

void generateUnusedImportFixes(const lsp::DocumentUri& uri, const Luau::LintWarning& lint, const TextDocument& textDocument,
    Luau::AstStatBlock* root, const std::optional<lsp::Diagnostic>& diagnostic, std::vector<lsp::CodeAction>& result)
{
    // Get the import name from the lint location
    lsp::Range lintRange = textDocument.convertLocation(lint.location);
    std::string importName = textDocument.getText(lintRange);

    // Fix 1: Prefix with '_' to silence the lint
    {
        lsp::CodeAction action;
        action.title = "Prefix '" + importName + "' with '_' to silence";
        action.kind = lsp::CodeActionKind::QuickFix;
        action.isPreferred = false;

        if (diagnostic)
            action.diagnostics.push_back(*diagnostic);

        lsp::Position insertPos = textDocument.convertPosition(lint.location.begin);
        lsp::TextEdit edit{{insertPos, insertPos}, "_"};

        lsp::WorkspaceEdit workspaceEdit;
        workspaceEdit.changes.emplace(uri, std::vector{edit});
        action.edit = workspaceEdit;

        result.push_back(action);
    }

    // Fix 2: Delete the import statement
    {
        auto statement = findStatementContainingLocal(root, lint.location);

        if (statement)
        {
            lsp::CodeAction action;
            action.title = "Remove unused import: '" + importName + "'";
            action.kind = lsp::CodeActionKind::QuickFix;
            action.isPreferred = false;

            if (diagnostic)
                action.diagnostics.push_back(*diagnostic);

            // Delete the entire line containing the import
            lsp::Range deleteRange{
                {statement->location.begin.line, 0},
                {statement->location.end.line + 1, 0}
            };
            lsp::TextEdit edit{deleteRange, ""};

            lsp::WorkspaceEdit workspaceEdit;
            workspaceEdit.changes.emplace(uri, std::vector{edit});
            action.edit = workspaceEdit;

            result.push_back(action);
        }
    }
}

void generateUnreachableCodeFix(const lsp::DocumentUri& uri, const Luau::LintWarning& lint, const TextDocument& textDocument,
    const std::optional<lsp::Diagnostic>& diagnostic, std::vector<lsp::CodeAction>& result)
{
    lsp::CodeAction action;
    action.title = "Remove unreachable code";
    action.kind = lsp::CodeActionKind::QuickFix;
    action.isPreferred = false;

    if (diagnostic)
        action.diagnostics.push_back(*diagnostic);

    // Delete the entire unreachable statement (the lint location is the unreachable statement)
    lsp::Range deleteRange{
        {lint.location.begin.line, 0},
        {lint.location.end.line + 1, 0}
    };
    lsp::TextEdit edit{deleteRange, ""};

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

        auto sourceModule = frontend.getSourceModule(moduleName);
        if (!sourceModule)
            return result;

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
            case Luau::LintWarning::Code_LocalUnused:
                generateUnusedVariableFixes(params.textDocument.uri, lint, *textDocument, sourceModule->root, diagnostic, result);
                break;
            case Luau::LintWarning::Code_FunctionUnused:
                generateUnusedFunctionFixes(params.textDocument.uri, lint, *textDocument, sourceModule->root, diagnostic, result);
                break;
            case Luau::LintWarning::Code_ImportUnused:
                generateUnusedImportFixes(params.textDocument.uri, lint, *textDocument, sourceModule->root, diagnostic, result);
                break;
            case Luau::LintWarning::Code_UnreachableCode:
                generateUnreachableCodeFix(params.textDocument.uri, lint, *textDocument, diagnostic, result);
                break;
            case Luau::LintWarning::Code_RedundantNativeAttribute:
            {
                lsp::CodeAction action;
                action.title = "Remove redundant @native attribute";
                action.kind = lsp::CodeActionKind::QuickFix;
                action.isPreferred = false;

                if (diagnostic)
                    action.diagnostics.push_back(*diagnostic);

                // Delete the entire line containing the @native attribute
                lsp::Range deleteRange{
                    {lint.location.begin.line, 0},
                    {lint.location.end.line + 1, 0}
                };
                lsp::TextEdit edit{deleteRange, ""};

                lsp::WorkspaceEdit workspaceEdit;
                workspaceEdit.changes.emplace(params.textDocument.uri, std::vector{edit});
                action.edit = workspaceEdit;

                result.push_back(action);
                break;
            }
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

        // Add "Remove all unused code" source action
        Luau::CheckResult cr =
            FFlag::LuauSolverV2 ? checkStrict(moduleName, cancellationToken, /* forAutocomplete= */ false) : checkSimple(moduleName, cancellationToken);
        throwIfCancelled(cancellationToken);

        auto sourceModule = frontend.getSourceModule(moduleName);
        if (sourceModule)
        {
            std::vector<lsp::TextEdit> edits;
            std::set<size_t> deletedLines; // Track which lines we've already deleted to avoid duplicates

            for (const auto& lint : cr.lintResult.warnings)
            {
                if (lint.code == Luau::LintWarning::Code_LocalUnused ||
                    lint.code == Luau::LintWarning::Code_FunctionUnused ||
                    lint.code == Luau::LintWarning::Code_ImportUnused)
                {
                    auto statement = findStatementContainingLocal(sourceModule->root, lint.location);
                    if (statement)
                    {
                        size_t startLine = statement->location.begin.line;
                        // Only add if we haven't already deleted this line
                        if (deletedLines.find(startLine) == deletedLines.end())
                        {
                            lsp::Range deleteRange{
                                {statement->location.begin.line, 0},
                                {statement->location.end.line + 1, 0}
                            };
                            edits.push_back({deleteRange, ""});

                            // Mark all lines in this range as deleted
                            for (size_t line = statement->location.begin.line; line <= statement->location.end.line; ++line)
                                deletedLines.insert(line);
                        }
                    }
                }
                else if (lint.code == Luau::LintWarning::Code_UnreachableCode)
                {
                    size_t startLine = lint.location.begin.line;
                    // Only add if we haven't already deleted this line
                    if (deletedLines.find(startLine) == deletedLines.end())
                    {
                        lsp::Range deleteRange{
                            {lint.location.begin.line, 0},
                            {lint.location.end.line + 1, 0}
                        };
                        edits.push_back({deleteRange, ""});

                        // Mark all lines in this range as deleted
                        for (size_t line = lint.location.begin.line; line <= lint.location.end.line; ++line)
                            deletedLines.insert(line);
                    }
                }
            }

            if (!edits.empty())
            {
                lsp::CodeAction removeUnusedAction;
                removeUnusedAction.title = "Remove all unused code";
                removeUnusedAction.kind = lsp::CodeActionKind::Source;

                lsp::WorkspaceEdit workspaceEdit;
                workspaceEdit.changes.emplace(params.textDocument.uri, edits);
                removeUnusedAction.edit = workspaceEdit;

                result.emplace_back(removeUnusedAction);
            }
        }
    }

    platform->handleCodeAction(params, result);

    return result;
}
