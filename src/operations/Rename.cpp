#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"

#include "LSP/LuauExt.hpp"

static bool isGlobalBinding(const WorkspaceFolder* workspaceFolder, const lsp::TextDocumentPositionParams& params)
{
    auto moduleName = workspaceFolder->fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = workspaceFolder->fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());
    auto position = textDocument->convertPosition(params.position);

    auto sourceModule = workspaceFolder->frontend.getSourceModule(moduleName);
    // TODO: fix "forAutocomplete"
    auto module = workspaceFolder->getModule(moduleName, /* forAutocomplete: */ true);
    auto binding = Luau::findBindingAtPosition(*module, *sourceModule, position);
    if (binding)
        return binding->location.begin == Luau::Position{0, 0} && binding->location.end == Luau::Position{0, 0};

    return false;
}

lsp::RenameResult WorkspaceFolder::rename(const lsp::RenameParams& params)
{
    // Verify the new name is valid (is an identifier)
    if (params.newName.length() == 0)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "The new name must be a valid identifier");
    if (!isalpha(params.newName.at(0)) && params.newName.at(0) != '_')
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "The new name must be a valid identifier starting with a character or underscore");

    if (!Luau::isIdentifier(params.newName))
        throw JsonRpcException(
            lsp::ErrorCode::RequestFailed, "The new name must be a valid identifier composed of characters, digits, and underscores only");

    // Use findAllReferences to determine locations
    lsp::ReferenceParams referenceParams{};
    referenceParams.textDocument = params.textDocument;
    referenceParams.position = params.position;
    referenceParams.context.includeDeclaration = true;
    auto references = this->references(referenceParams);

    if (!references)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "Unable to find symbol to rename");

    if (isGlobalBinding(this, params))
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "Cannot rename a global variable");

    lsp::WorkspaceEdit result{};

    for (const auto& reference : *references)
    {
        if (!contains(result.changes, reference.uri.toString()))
            result.changes.insert_or_assign(reference.uri.toString(), std::vector<lsp::TextEdit>{});

        result.changes.at(reference.uri.toString()).emplace_back(lsp::TextEdit{reference.range, params.newName});
    }

    return result;
}

lsp::RenameResult LanguageServer::rename(const lsp::RenameParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->rename(params);
}

static lsp::PrepareRenameRangePlaceholderResult createRangePlaceholder(const TextDocument& textDocument, Luau::Location location)
{
    lsp::Range range = {textDocument.convertPosition(location.begin), textDocument.convertPosition(location.end)};
    auto text = textDocument.getText(range);
    return {range, /* placeholder: */ text};
}

static std::optional<Luau::Location> findTypeReferenceLocation(
    Luau::Position position, Luau::AstArray<Luau::AstGenericType> generics, Luau::AstArray<Luau::AstGenericTypePack> genericPacks)
{
    for (const auto t : generics)
        if (t.location.containsClosed(position))
            return t.location;
    for (const auto t : genericPacks)
        if (t.location.containsClosed(position))
            return t.location;
    return std::nullopt;
}

// TODO: Make sure this is correct.
lsp::PrepareRenameResult WorkspaceFolder::prepareRename(const lsp::PrepareRenameParams& params)
{
    if (isGlobalBinding(this, params))
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "Cannot rename a global variable");

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());
    auto position = textDocument->convertPosition(params.position);

    // Run the type checker to ensure we are up to date
    // We check for autocomplete here since autocomplete has stricter types
    // checkStrict(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "Unable to read source code");

    auto exprOrLocal = findExprOrLocalAtPositionClosed(*sourceModule, position);
    if (exprOrLocal.getLocal())
        return createRangePlaceholder(*textDocument, exprOrLocal.getLocal()->location);
    else if (auto expr = exprOrLocal.getExpr())
    {
        if (auto local = expr->as<Luau::AstExprLocal>())
            return createRangePlaceholder(*textDocument, local->location);
        else if (auto global = expr->as<Luau::AstExprGlobal>())
            return createRangePlaceholder(*textDocument, global->location);
    }

    auto module = getModule(moduleName, /* forAutocomplete: */ true);
    if (auto expr = exprOrLocal.getExpr())
    {
        if (auto indexName = expr->as<Luau::AstExprIndexName>())
            return createRangePlaceholder(*textDocument, indexName->indexLocation);
        else if (auto constantString = expr->as<Luau::AstExprConstantString>())
        {
            // Potentially a property defined inside of a table
            auto ancestry = Luau::findAstAncestryOfPosition(*sourceModule, position, /* includeTypes = */ false);
            if (ancestry.size() > 1)
            {
                auto parent = ancestry.at(ancestry.size() - 2);
                if (auto tbl = parent->as<Luau::AstExprTable>())
                    return createRangePlaceholder(*textDocument, constantString->location);
            }
        }
    }

    // Search for a type reference
    auto node = findNodeOrTypeAtPositionClosed(*sourceModule, position);
    if (!node)
        return std::nullopt;

    if (auto typeDefinition = node->as<Luau::AstStatTypeAlias>(); typeDefinition && typeDefinition->nameLocation.containsClosed(position))
        return createRangePlaceholder(*textDocument, typeDefinition->nameLocation);
    else if (auto reference = node->as<Luau::AstTypeReference>(); reference && reference->nameLocation.containsClosed(position))
        return createRangePlaceholder(*textDocument, reference->nameLocation);
    else if (auto typeFunction = node->as<Luau::AstTypeFunction>())
        if (auto location = findTypeReferenceLocation(position, typeFunction->generics, typeFunction->genericPacks))
            return createRangePlaceholder(*textDocument, *location);

    return std::nullopt;
}

lsp::PrepareRenameResult LanguageServer::prepareRename(const lsp::PrepareRenameParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->prepareRename(params);
}
