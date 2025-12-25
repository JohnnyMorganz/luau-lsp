#include "LSP/Workspace.hpp"

#include "Luau/AstQuery.h"
#include "LSP/LuauExt.hpp"

static std::optional<Luau::Binding> getBinding(
    const WorkspaceFolder* workspaceFolder, const Luau::ModuleName& moduleName, const Luau::Position& position)
{
    auto sourceModule = workspaceFolder->frontend.getSourceModule(moduleName);
    // TODO: fix "forAutocomplete"
    auto module = workspaceFolder->getModule(moduleName, /* forAutocomplete: */ true);
    auto binding = Luau::findBindingAtPosition(*module, *sourceModule, position);
    return binding;
}

static bool isGlobalBinding(const Luau::Binding& binding)
{
    return binding.location.begin == Luau::Position{0, 0} && binding.location.end == Luau::Position{0, 0};
}

std::vector<lsp::Location> getReferencesForRenaming(
    WorkspaceFolder* workspaceFolder, const lsp::RenameParams& params, const LSPCancellationToken& cancellationToken)
{
    auto moduleName = workspaceFolder->fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = workspaceFolder->fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());
    auto position = textDocument->convertPosition(params.position);
    workspaceFolder->checkStrict(moduleName, cancellationToken);
    throwIfCancelled(cancellationToken);

    auto sourceModule = workspaceFolder->frontend.getSourceModule(moduleName);
    if (!sourceModule)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "Unable to read source code");

    if (auto binding = getBinding(workspaceFolder, moduleName, position); binding && isGlobalBinding(*binding))
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "Cannot rename a global variable");

    // Find All References can return cross module references of a symbol
    // If this is a local symbol in a file, then just rename that instead
    auto exprOrLocal = findExprOrLocalAtPositionClosed(*sourceModule, position);

    Luau::Symbol symbol;
    if (exprOrLocal.getLocal())
        symbol = exprOrLocal.getLocal();
    else if (auto expr = exprOrLocal.getExpr())
    {
        if (auto local = expr->as<Luau::AstExprLocal>())
            symbol = local->local;
        else if (auto global = expr->as<Luau::AstExprGlobal>())
            symbol = global->name;
    }

    if (symbol)
    {
        std::vector<lsp::Location> result;

        auto references = findSymbolReferences(*sourceModule, symbol);
        result.reserve(references.size());
        for (auto& location : references)
        {
            result.emplace_back(
                lsp::Location{params.textDocument.uri, {textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)}});
        }

        return result;
    }
    else
    {
        // Use findAllReferences to determine locations
        lsp::ReferenceParams referenceParams{};
        referenceParams.textDocument = params.textDocument;
        referenceParams.position = params.position;
        referenceParams.context.includeDeclaration = true;
        return workspaceFolder->references(referenceParams, cancellationToken).value_or(std::vector<lsp::Location>{});
    }
}

/// Checks if the given location refers to a quoted string literal that needs range adjustment during rename.
/// If so, returns an adjusted range that excludes the quotes (so the edit replaces only the content inside).
/// For table keys, only adjusts if it's bracket notation (General kind), not shorthand syntax.
static std::optional<lsp::Range> getAdjustedRangeIfQuotedString(WorkspaceFolder* workspaceFolder, const lsp::Location& location)
{
    auto moduleName = workspaceFolder->fileResolver.getModuleName(location.uri);
    auto sourceModule = workspaceFolder->frontend.getSourceModule(moduleName);
    auto textDocument = workspaceFolder->fileResolver.getOrCreateTextDocumentFromModuleName(moduleName);
    if (!sourceModule || !textDocument)
        return std::nullopt;

    auto position = textDocument->convertPosition(location.range.start);

    auto ancestry = Luau::findAstAncestryOfPosition(*sourceModule, position, /* includeTypes= */ false);
    if (ancestry.size() < 2)
        return std::nullopt;

    auto node = ancestry.back();
    auto constantString = node->as<Luau::AstExprConstantString>();
    if (!constantString)
        return std::nullopt;

    // Check if this string is a table key - if so, we need to verify it's bracket notation (General kind)
    auto parent = ancestry.at(ancestry.size() - 2);
    if (auto table = parent->as<Luau::AstExprTable>())
    {
        for (const auto& item : table->items)
        {
            if (item.key == node)
            {
                if (item.kind != Luau::AstExprTable::Item::Kind::General)
                    return std::nullopt;
                break;
            }
        }
    }

    return lsp::Range{{location.range.start.line, location.range.start.character + 1}, {location.range.end.line, location.range.end.character - 1}};
}

lsp::RenameResult WorkspaceFolder::rename(const lsp::RenameParams& params, const LSPCancellationToken& cancellationToken)
{
    // Verify the new name is valid (is an identifier)
    if (params.newName.length() == 0)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "The new name must be a valid identifier");
    if (!isalpha(params.newName.at(0)) && params.newName.at(0) != '_')
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "The new name must be a valid identifier starting with a character or underscore");

    if (!Luau::isIdentifier(params.newName))
        throw JsonRpcException(
            lsp::ErrorCode::RequestFailed, "The new name must be a valid identifier composed of characters, digits, and underscores only");

    auto references = getReferencesForRenaming(this, params, cancellationToken);
    if (references.empty())
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "Unable to find symbol to rename");

    lsp::WorkspaceEdit result{};

    for (const auto& reference : references)
    {
        if (result.changes.find(reference.uri) == result.changes.end())
            result.changes.insert_or_assign(reference.uri, std::vector<lsp::TextEdit>{});

        // For quoted strings (bracket notation), adjust range to exclude quotes
        lsp::Range editRange = reference.range;
        if (auto adjustedRange = getAdjustedRangeIfQuotedString(this, reference))
            editRange = *adjustedRange;

        result.changes.at(reference.uri).emplace_back(lsp::TextEdit{editRange, params.newName});
    }

    return result;
}
