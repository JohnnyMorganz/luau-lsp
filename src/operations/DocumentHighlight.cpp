#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"

#include "LSP/LuauExt.hpp"

static lsp::DocumentHighlight createHighlight(const TextDocument& textDocument, Luau::Location location, lsp::DocumentHighlightKind kind)
{
    return lsp::DocumentHighlight{{textDocument.convertPosition(location.begin), textDocument.convertPosition(location.end)}, kind};
}

static std::vector<lsp::DocumentHighlight> findAllPropertyHighlights(
    const TextDocument& textDocument, Luau::ModulePtr module, const Luau::SourceModule& source, Luau::TypeId ty, Luau::Name property)
{
    ty = Luau::follow(ty);
    auto ttv = Luau::get<Luau::TableType>(ty);

    if (!ttv || ttv->definitionModuleName.empty())
        return {};

    std::vector<lsp::DocumentHighlight> highlights;

    auto [locations, kinds] = findPropertyReferencesWithKinds(source, property, ty, module->astTypes);
    highlights.reserve(locations.size());

    for (size_t i = 0; i < locations.size(); ++i)
    {
        highlights.emplace_back(createHighlight(textDocument, locations[i], kinds[i]));
    }

    return highlights;
}

// Determines whether the name matches a type reference in one of the provided generics
// If so, we find the usages inside of that node
static bool handleIfTypeReferenceByName(Luau::AstNode* node, Luau::AstArray<Luau::AstGenericType> generics,
    Luau::AstArray<Luau::AstGenericTypePack> genericPacks, Luau::AstName name, std::vector<lsp::DocumentHighlight>& highlights,
    const TextDocument* textDocument)
{
    if (!isTypeReference(name, generics, genericPacks))
        return false;

    auto [locations, kinds] = findTypeParameterUsagesWithKinds(*node, name);

    highlights.reserve(highlights.size() + locations.size());

    for (size_t i = 0; i < locations.size(); ++i)
    {
        highlights.push_back(createHighlight(*textDocument, locations[i], kinds[i]));
    }

    return true;
}

// Determines whether the name matches a type reference in one of the provided generics
// If so, we find the usages inside of that node
static bool handleIfTypeReferenceByPosition(Luau::AstNode* node, Luau::AstArray<Luau::AstGenericType> generics,
    Luau::AstArray<Luau::AstGenericTypePack> genericPacks, Luau::Position position, std::vector<lsp::DocumentHighlight>& highlights,
    const TextDocument* textDocument)
{
    auto name = findTypeReferenceName(position, generics, genericPacks);
    if (!name)
        return false;

    auto [locations, kinds] = findTypeParameterUsagesWithKinds(*node, name.value());

    highlights.reserve(highlights.size() + locations.size());

    for (size_t i = 0; i < locations.size(); ++i)
    {
        highlights.push_back(createHighlight(*textDocument, locations[i], kinds[i]));
    }

    return true;
}

lsp::DocumentHighlightResult WorkspaceFolder::documentHighlight(const lsp::DocumentHighlightParams& params)
{
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

    // Search for usages of a local or global symbol
    if (symbol)
    {
        auto [locations, kinds] = findSymbolReferencesWithKinds(*sourceModule, symbol);

        std::vector<lsp::DocumentHighlight> highlights{};
        highlights.reserve(locations.size());
        for (size_t i = 0; i < locations.size(); ++i)
        {
            highlights.emplace_back(createHighlight(*textDocument, locations[i], kinds[i]));
        }

        return highlights;
    }

    // Search for a property
    auto module = getModule(moduleName, /* forAutocomplete: */ true);
    if (auto expr = exprOrLocal.getExpr())
    {
        if (auto indexName = expr->as<Luau::AstExprIndexName>())
        {
            auto possibleParentTy = module->astTypes.find(indexName->expr);
            if (possibleParentTy)
            {
                auto parentTy = Luau::follow(*possibleParentTy);
                return findAllPropertyHighlights(*textDocument, module, *sourceModule, parentTy, indexName->index.value);
            }
        }
        else if (auto constantString = expr->as<Luau::AstExprConstantString>())
        {
            // Potentially a property defined inside of a table
            auto ancestry = Luau::findAstAncestryOfPosition(*sourceModule, position, /* includeTypes = */ false);
            if (ancestry.size() > 1)
            {
                auto parent = ancestry.at(ancestry.size() - 2);
                if (auto tbl = parent->as<Luau::AstExprTable>())
                {
                    auto possibleTableTy = module->astTypes.find(tbl);
                    if (possibleTableTy)
                    {
                        return findAllPropertyHighlights(*textDocument, module, *sourceModule, Luau::follow(*possibleTableTy),
                            Luau::Name(constantString->value.data, constantString->value.size));
                    }
                }
            }
        }
    }

    // Search for a type reference
    auto node = findNodeOrTypeAtPositionClosed(*sourceModule, position);
    if (!node)
        return std::nullopt;

    std::vector<lsp::DocumentHighlight> highlights;

    if (auto typeDefinition = node->as<Luau::AstStatTypeAlias>())
    {
        // Check to see whether the position was actually the type parameter: "S" in `type State<S> = ...`
        if (handleIfTypeReferenceByPosition(
                typeDefinition, typeDefinition->generics, typeDefinition->genericPacks, position, highlights, textDocument))
        {
            return highlights;
        }

        // Include all usages of the type
        auto references = findTypeReferences(*sourceModule, typeDefinition->name.value, std::nullopt);
        highlights.reserve(references.size() + 1);
        for (auto& location : references)
            highlights.emplace_back(createHighlight(*textDocument, location, lsp::DocumentHighlightKind::Read));

        // Include the type definition
        highlights.emplace_back(createHighlight(*textDocument, typeDefinition->nameLocation, lsp::DocumentHighlightKind::Write));

        return highlights;
    }
    else if (auto reference = node->as<Luau::AstTypeReference>())
    {
        if (auto prefix = reference->prefix)
        {
            auto requireInfo = findClosestAncestorModuleImport(*sourceModule, reference->prefix.value(), reference->prefixLocation->begin);
            if (!requireInfo)
                return std::nullopt;

            auto requireSymbol = requireInfo.value().first;

            if (reference->prefixLocation.value().containsClosed(position))
            {
                auto [locations, kinds] = findSymbolReferencesWithKinds(*sourceModule, requireSymbol);
                highlights.reserve(locations.size());

                for (size_t i = 0; i < locations.size(); ++i)
                {
                    highlights.emplace_back(createHighlight(*textDocument, locations[i], kinds[i]));
                }
            }
            else
            {
                auto references = findTypeReferences(*sourceModule, reference->name.value, reference->prefix.value().value);
                highlights.reserve(references.size());

                for (auto& location : references)
                {
                    highlights.emplace_back(createHighlight(*textDocument, location, lsp::DocumentHighlightKind::Read));
                }
            }

            return highlights;
        }
        else
        {
            // This could potentially be a generic type parameter - so we want to find its references if so.
            auto ancestry = Luau::findAstAncestryOfPosition(*sourceModule, position, /* includeTypes= */ true);
            for (auto it = ancestry.rbegin(); it != ancestry.rend(); ++it)
            {
                if (auto typeAlias = (*it)->as<Luau::AstStatTypeAlias>())
                {
                    if (handleIfTypeReferenceByName(
                            typeAlias, typeAlias->generics, typeAlias->genericPacks, reference->name, highlights, textDocument))
                        return highlights;
                    break;
                }
                else if (auto typeFunction = (*it)->as<Luau::AstTypeFunction>())
                {
                    if (handleIfTypeReferenceByName(
                            typeFunction, typeFunction->generics, typeFunction->genericPacks, reference->name, highlights, textDocument))
                        return highlights;
                }
                else if (auto func = (*it)->as<Luau::AstExprFunction>())
                {
                    if (handleIfTypeReferenceByName(func, func->generics, func->genericPacks, reference->name, highlights, textDocument))
                        return highlights;
                    break;
                }
                else if (!(*it)->asType())
                {
                    // No longer inside a type, so no point going further
                    break;
                }
            }

            auto references = findTypeReferences(*sourceModule, reference->name.value, std::nullopt);
            highlights.reserve(references.size() + 1);
            for (auto& location : references)
                highlights.emplace_back(createHighlight(*textDocument, location, lsp::DocumentHighlightKind::Read));

            // Find the actual declaration location
            auto scope = Luau::findScopeAtPosition(*module, position);
            while (scope)
            {
                if (auto location = scope->typeAliasNameLocations.find(reference->name.value); location != scope->typeAliasNameLocations.end())
                {
                    highlights.emplace_back(createHighlight(*textDocument, location->second, lsp::DocumentHighlightKind::Write));
                    break;
                }

                scope = scope->parent;
            }

            return highlights;
        }
    }
    else if (auto typeFunction = node->as<Luau::AstTypeFunction>())
    {
        if (handleIfTypeReferenceByPosition(typeFunction, typeFunction->generics, typeFunction->genericPacks, position, highlights, textDocument))
        {
            return highlights;
        }
    }
    else if (auto func = node->as<Luau::AstExprFunction>())
    {
        if (handleIfTypeReferenceByPosition(func, func->generics, func->genericPacks, position, highlights, textDocument))
        {
            return highlights;
        }
    }

    return std::nullopt;
}

lsp::DocumentHighlightResult LanguageServer::documentHighlight(const lsp::DocumentHighlightParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->documentHighlight(params);
}