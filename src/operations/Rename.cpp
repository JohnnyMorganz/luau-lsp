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

static void buildResult(
    lsp::WorkspaceEdit& result, const std::string& newName, const std::vector<Luau::Location>& locations, const TextDocument* textDocument)
{
    if (!contains(result.changes, textDocument->uri().toString()))
        result.changes.insert_or_assign(textDocument->uri().toString(), std::vector<lsp::TextEdit>{});

    for (const auto& location : locations)
        result.changes.at(textDocument->uri().toString())
            .emplace_back(lsp::TextEdit{{textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)}, newName});
}

class FindTypeParameterUsages : public Luau::AstVisitor
{
    Luau::AstName name;
    bool initialNode = true;

    bool visit(class Luau::AstType* node) override
    {
        return true;
    }

    bool visit(class Luau::AstTypeReference* node) override
    {
        if (node->name == name)
            result.emplace_back(node->nameLocation);
        initialNode = false;
        return true;
    }

    bool visit(class Luau::AstTypeFunction* node) override
    {
        // Check to see the type parameter has not been redefined
        for (auto t : node->generics)
        {
            if (t.name == name)
            {
                if (initialNode)
                    result.emplace_back(t.location);
                else
                    return false;
            }
        }
        for (auto t : node->genericPacks)
        {
            if (t.name == name)
            {
                if (initialNode)
                    result.emplace_back(t.location);
                else
                    return false;
            }
        }
        initialNode = false;
        return true;
    }

    bool visit(class Luau::AstTypePack* node) override
    {
        return true;
    }

    bool visit(class Luau::AstTypePackGeneric* node) override
    {
        if (node->genericName == name)
            // node location also consists of the three dots "...", so we need to remove them
            result.emplace_back(Luau::Location{node->location.begin, {node->location.end.line, node->location.end.column - 3}});
        initialNode = false;
        return true;
    }

    bool visit(class Luau::AstStatTypeAlias* node) override
    {
        for (auto t : node->generics)
            if (t.name == name)
                result.emplace_back(t.location);
        for (auto t : node->genericPacks)
            if (t.name == name)
                result.emplace_back(t.location);
        initialNode = false;
        return true;
    }

    bool visit(class Luau::AstExprFunction* node) override
    {
        for (auto t : node->generics)
            if (t.name == name)
                result.emplace_back(t.location);
        for (auto t : node->genericPacks)
            if (t.name == name)
                result.emplace_back(t.location);
        initialNode = false;
        return true;
    }

public:
    FindTypeParameterUsages(Luau::AstName name)
        : name(name)
    {
    }
    std::vector<Luau::Location> result;
};


// Determines whether the name matches a type reference in one of the provided generics
// If so, we find the usages inside of that node and construct the rename list
bool handleIfTypeReferenceByName(Luau::AstNode* node, Luau::AstArray<Luau::AstGenericType> generics,
    Luau::AstArray<Luau::AstGenericTypePack> genericPacks, Luau::AstName name, lsp::WorkspaceEdit& result, const std::string& newName,
    const TextDocument* textDocument)
{
    bool isTypeReference = false;
    for (const auto t : generics)
    {
        if (t.name == name)
        {
            isTypeReference = true;
            break;
        }
    }
    for (const auto t : genericPacks)
    {
        if (t.name == name)
        {
            isTypeReference = true;
            break;
        }
    }
    if (!isTypeReference)
        return false;

    FindTypeParameterUsages visitor(name);
    node->visit(&visitor);
    buildResult(result, newName, visitor.result, textDocument);

    return true;
}

// Determines whether the name matches a type reference in one of the provided generics
// If so, we find the usages inside of that node and construct the rename list
bool handleIfTypeReferenceByPosition(Luau::AstNode* node, Luau::AstArray<Luau::AstGenericType> generics,
    Luau::AstArray<Luau::AstGenericTypePack> genericPacks, Luau::Position position, lsp::WorkspaceEdit& result, const std::string& newName,
    const TextDocument* textDocument)
{
    Luau::AstName name;
    bool isTypeReference = false;
    for (const auto t : generics)
    {
        if (t.location.containsClosed(position))
        {
            name = t.name;
            isTypeReference = true;
            break;
        }
    }
    for (const auto t : genericPacks)
    {
        if (t.location.containsClosed(position))
        {
            name = t.name;
            isTypeReference = true;
            break;
        }
    }
    if (!isTypeReference)
        return false;

    FindTypeParameterUsages visitor(name);
    node->visit(&visitor);
    buildResult(result, newName, visitor.result, textDocument);

    return true;
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
        std::vector<lsp::TextEdit> localChanges;
        localChanges.reserve(references.size());
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

    if (auto typeDefinition = node->as<Luau::AstStatTypeAlias>())
    {
        // Check to see whether the position was actually the type parameter: "S" in `type State<S> = ...`
        if (handleIfTypeReferenceByPosition(
                typeDefinition, typeDefinition->generics, typeDefinition->genericPacks, position, result, params.newName, textDocument))
        {
            return result;
        }

        if (typeDefinition->exported)
        {
            // Type may potentially be used in other files, so we need to handle this globally
            auto references = findAllTypeReferences(moduleName, typeDefinition->name.value);
            processReferences(fileResolver, params.newName, references, result);
            return result;
        }
        else
        {
            std::vector<lsp::TextEdit> localChanges{};

            // Update the type definition
            localChanges.emplace_back(lsp::TextEdit{
                {textDocument->convertPosition(typeDefinition->nameLocation.begin), textDocument->convertPosition(typeDefinition->nameLocation.end)},
                params.newName});

            // Update all usages of the type
            auto references = findTypeReferences(*sourceModule, typeDefinition->name.value, std::nullopt);
            localChanges.reserve(references.size() + 1);
            for (auto& location : references)
                localChanges.emplace_back(
                    lsp::TextEdit{{textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)}, params.newName});

            result.changes.insert_or_assign(params.textDocument.uri.toString(), localChanges);
            return result;
        }
    }
    else if (auto reference = node->as<Luau::AstTypeReference>())
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
            // This could potentially be a generic type - so we want to rename that if so.
            auto ancestry = Luau::findAstAncestryOfPosition(*sourceModule, position, /* includeTypes= */ true);
            for (auto it = ancestry.rbegin(); it != ancestry.rend(); ++it)
            {
                if (auto typeAlias = (*it)->as<Luau::AstStatTypeAlias>())
                {
                    if (handleIfTypeReferenceByName(
                            typeAlias, typeAlias->generics, typeAlias->genericPacks, reference->name, result, params.newName, textDocument))
                        return result;
                    break;
                }
                else if (auto typeFunction = (*it)->as<Luau::AstTypeFunction>())
                {
                    if (handleIfTypeReferenceByName(
                            typeFunction, typeFunction->generics, typeFunction->genericPacks, reference->name, result, params.newName, textDocument))
                        return result;
                }
                else if (auto func = (*it)->as<Luau::AstExprFunction>())
                {
                    if (handleIfTypeReferenceByName(func, func->generics, func->genericPacks, reference->name, result, params.newName, textDocument))
                        return result;
                    break;
                }
                else if (!(*it)->asType())
                {
                    // No longer inside a type, so no point going further
                    break;
                }
            }

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
    else if (auto typeFunction = node->as<Luau::AstTypeFunction>())
    {
        if (handleIfTypeReferenceByPosition(
                typeFunction, typeFunction->generics, typeFunction->genericPacks, position, result, params.newName, textDocument))
        {
            return result;
        }
    }
    else if (auto func = node->as<Luau::AstExprFunction>())
    {
        if (handleIfTypeReferenceByPosition(func, func->generics, func->genericPacks, position, result, params.newName, textDocument))
        {
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
