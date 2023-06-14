#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"

#include "LSP/LuauExt.hpp"

static void processReferences(
    WorkspaceFileResolver& fileResolver, const std::string& newName, const std::vector<Reference>& references, lsp::WorkspaceEdit& result)
{
    for (const auto& reference : references)
    {
        if (auto refTextDocument = fileResolver.getTextDocumentFromModuleName(reference.moduleName))
        {
            // Create a vector of changes if it does not yet exist
            if (!contains(result.changes, refTextDocument->uri().toString()))
                result.changes.insert_or_assign(refTextDocument->uri().toString(), std::vector<lsp::TextEdit>{});

            result.changes.at(refTextDocument->uri().toString())
                .emplace_back(lsp::TextEdit{
                    {refTextDocument->convertPosition(reference.location.begin), refTextDocument->convertPosition(reference.location.end)}, newName});
        }
        else
        {
            if (auto filePath = fileResolver.resolveToRealPath(reference.moduleName))
            {
                if (auto source = fileResolver.readSource(reference.moduleName))
                {
                    auto refTextDocument = TextDocument{Uri::file(*filePath), "luau", 0, source->source};
                    // Create a vector of changes if it does not yet exist
                    if (!contains(result.changes, refTextDocument.uri().toString()))
                        result.changes.insert_or_assign(refTextDocument.uri().toString(), std::vector<lsp::TextEdit>{});

                    result.changes.at(refTextDocument.uri().toString())
                        .emplace_back(lsp::TextEdit{
                            {refTextDocument.convertPosition(reference.location.begin), refTextDocument.convertPosition(reference.location.end)},
                            newName});
                }
            }
        }
    }
}

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

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());
    auto position = textDocument->convertPosition(params.position);

    // Run the type checker to ensure we are up to date
    // We check for autocomplete here since autocomplete has stricter types
    checkStrict(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "Unable to read source code");

    auto exprOrLocal = findExprOrLocalAtPositionClosed(*sourceModule, position);
    Luau::Symbol localSymbol;
    if (exprOrLocal.getLocal())
        localSymbol = exprOrLocal.getLocal();
    else if (auto expr = exprOrLocal.getExpr())
    {
        if (auto local = expr->as<Luau::AstExprLocal>())
            localSymbol = local->local;
    }

    lsp::WorkspaceEdit result{};

    if (localSymbol)
    {
        // Search for a local binding
        auto references = findSymbolReferences(*sourceModule, localSymbol);
        std::vector<lsp::TextEdit> localChanges{};
        for (auto& location : references)
        {
            localChanges.emplace_back(
                lsp::TextEdit{{textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)}, params.newName});
        }
        result.changes.insert_or_assign(params.textDocument.uri.toString(), localChanges);

        return result;
    }

    // Search for a property
    auto module = frontend.moduleResolverForAutocomplete.getModule(moduleName);
    if (auto expr = exprOrLocal.getExpr())
    {
        if (auto indexName = expr->as<Luau::AstExprIndexName>())
        {
            auto possibleParentTy = module->astTypes.find(indexName->expr);
            if (possibleParentTy)
            {
                auto parentTy = Luau::follow(*possibleParentTy);
                auto references = findAllReferences(parentTy, indexName->index.value);
                processReferences(fileResolver, params.newName, references, result);
                return result;
            }
        }
    }

    // Search for a type reference
    auto node = findNodeOrTypeAtPositionClosed(*sourceModule, position);
    if (!node)
        return std::nullopt;

    if (auto reference = node->as<Luau::AstTypeReference>())
    {
        if (auto prefix = reference->prefix)
        {
            if (auto importedModuleName = module->getModuleScope()->importedModules.find(prefix.value().value);
                importedModuleName != module->getModuleScope()->importedModules.end())
            {
                auto references = findAllTypeReferences(importedModuleName->second, reference->name.value);
                processReferences(fileResolver, params.newName, references, result);
                return result;
            }

            return std::nullopt;
        }
        else
        {
            std::vector<lsp::TextEdit> localChanges{};
            auto references = findTypeReferences(*sourceModule, reference->name.value, std::nullopt);
            localChanges.reserve(references.size() + 1);
            for (auto& location : references)
                localChanges.emplace_back(
                    lsp::TextEdit{{textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)}, params.newName});

            // Find the actual declaration location
            auto scope = Luau::findScopeAtPosition(*module, position);
            while (scope)
            {
                if (auto location = scope->typeAliasNameLocations.find(reference->name.value); location != scope->typeAliasNameLocations.end())
                {
                    localChanges.emplace_back(
                        lsp::TextEdit{{textDocument->convertPosition(location->second.begin), textDocument->convertPosition(location->second.end)},
                            params.newName});
                    break;
                }

                scope = scope->parent;
            }

            result.changes.insert_or_assign(params.textDocument.uri.toString(), localChanges);

            return result;
        }
    }

    throw JsonRpcException(lsp::ErrorCode::RequestFailed, "Unable to find symbol to rename");
}

lsp::RenameResult LanguageServer::rename(const lsp::RenameParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->rename(params);
}