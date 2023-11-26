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

    // Use findAllReferences to determine locations
    lsp::ReferenceParams referenceParams{};
    referenceParams.textDocument = params.textDocument;
    referenceParams.position = params.position;
    referenceParams.context.includeDeclaration = true;
    auto references = this->references(referenceParams);

    if (!references)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "Unable to find symbol to rename");

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
