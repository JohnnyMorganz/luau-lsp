#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"

#include "LSP/LuauExt.hpp"

lsp::RenameResult WorkspaceFolder::rename(const lsp::RenameParams& params)
{
    // Verify the new name is valid (is an identifier)
    if (params.newName.length() == 0)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "The new name must be a valid identifier");
    if (!isalpha(params.newName.at(0)) && params.newName.at(0) != '_')
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "The new name must be a valid identifier starting with a character or underscore");
    for (auto ch : params.newName)
    {
        if (!isalpha(ch) && !isdigit(ch) && ch != '_')
            throw JsonRpcException(
                lsp::ErrorCode::RequestFailed, "The new name must be a valid identifier composed of characters, digits, and underscores only");
    }

    // TODO: currently we only support renaming local bindings in the current file
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());
    auto position = textDocument->convertPosition(params.position);

    // Run the type checker to ensure we are up to date
    // TODO: we only need the parse result here - can typechecking be skipped?
    if (frontend.isDirty(moduleName))
        frontend.check(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "Unable to read source code");

    auto exprOrLocal = findExprOrLocalAtPositionClosed(*sourceModule, position);
    if (!exprOrLocal.getLocal() && !exprOrLocal.getExpr())
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "Unable to find symbol. Ensure the variable is highlighted correctly");

    Luau::Symbol symbol;

    if (exprOrLocal.getLocal())
        symbol = exprOrLocal.getLocal();
    else if (auto exprLocal = exprOrLocal.getExpr()->as<Luau::AstExprLocal>())
        symbol = exprLocal->local;
    else
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "Rename is currently only supported for local variable bindings in the current file");

    auto references = findSymbolReferences(*sourceModule, symbol);
    std::vector<lsp::TextEdit> localChanges;
    for (auto& location : references)
    {
        localChanges.emplace_back(
            lsp::TextEdit{{textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)}, params.newName});
    }

    return lsp::WorkspaceEdit{{{params.textDocument.uri.toString(), localChanges}}};
}

lsp::RenameResult LanguageServer::rename(const lsp::RenameParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->rename(params);
}