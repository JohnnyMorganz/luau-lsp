#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"

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

std::vector<lsp::Location> getReferencesForRenaming(WorkspaceFolder* workspaceFolder, const lsp::RenameParams& params)
{
    auto moduleName = workspaceFolder->fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = workspaceFolder->fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());
    auto position = textDocument->convertPosition(params.position);
    workspaceFolder->checkStrict(moduleName);

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
        return workspaceFolder->references(referenceParams).value_or(std::vector<lsp::Location>{});
    }
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

    auto references = getReferencesForRenaming(this, params);
    if (references.empty())
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "Unable to find symbol to rename");

    lsp::WorkspaceEdit result{};

    for (const auto& reference : references)
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
