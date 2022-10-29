#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"

#include "Luau/AstQuery.h"
#include "LSP/LuauExt.hpp"

lsp::ReferenceResult WorkspaceFolder::references(const lsp::ReferenceParams& params)
{
    // TODO: currently we only support searching for a binding at a current position
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(moduleName);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + moduleName);

    auto position = textDocument->convertPosition(params.position);

    // Run the type checker to ensure we are up to date
    // TODO: we only need the parse result here - can typechecking be skipped?
    if (frontend.isDirty(moduleName))
        frontend.check(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return std::nullopt;

    auto exprOrLocal = Luau::findExprOrLocalAtPosition(*sourceModule, position);
    Luau::Symbol symbol;

    if (exprOrLocal.getLocal())
        symbol = exprOrLocal.getLocal();
    else if (auto exprLocal = exprOrLocal.getExpr()->as<Luau::AstExprLocal>())
        symbol = exprLocal->local;
    else
        return std::nullopt;

    auto references = findSymbolReferences(*sourceModule, symbol);
    std::vector<lsp::Location> result;

    for (auto& location : references)
    {
        result.emplace_back(
            lsp::Location{params.textDocument.uri, {textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)}});
    }

    return result;
}

lsp::ReferenceResult LanguageServer::references(const lsp::ReferenceParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->references(params);
}