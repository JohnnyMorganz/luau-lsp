#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"

#include "Luau/AstQuery.h"
#include "LSP/LuauExt.hpp"

// Find all reverse dependencies of the top-level module
// NOTE: this function is quite expensive as it requires a BFS
// TODO: this comes from `markDirty`
std::vector<Luau::ModuleName> WorkspaceFolder::findReverseDependencies(const Luau::ModuleName& moduleName)
{
    std::vector<Luau::ModuleName> dependents{};
    std::unordered_map<Luau::ModuleName, std::vector<Luau::ModuleName>> reverseDeps;
    for (const auto& module : frontend.sourceNodes)
    {
        for (const auto& dep : module.second->requireSet)
            reverseDeps[dep].push_back(module.first);
    }
    std::vector<Luau::ModuleName> queue{moduleName};
    while (!queue.empty())
    {
        Luau::ModuleName next = std::move(queue.back());
        queue.pop_back();

        // TODO: maybe do a set-lookup instead?
        if (contains(dependents, next))
            continue;

        dependents.push_back(next);

        if (0 == reverseDeps.count(next))
            continue;

        const std::vector<Luau::ModuleName>& dependents = reverseDeps[next];
        queue.insert(queue.end(), dependents.begin(), dependents.end());
    }
    return dependents;
}

// Find all references across all files for the usage of TableType, or a property on a TableType
std::vector<Reference> WorkspaceFolder::findAllReferences(Luau::TypeId ty, std::optional<Luau::Name> property)
{
    ty = Luau::follow(ty);
    auto ttv = Luau::get<Luau::TableType>(ty);

    // When the definition module name is starts with '@', e.g. "@luau" or "@roblox", `LUAU_ASSERT(!buildQueueItems.empty());` in Frontend.cpp fails,
    // after we call checkStrict below
    if (!ttv || ttv->definitionModuleName.empty() || ttv->definitionModuleName[0] == '@')
        return {};

    std::vector<Reference> references;
    std::vector<Luau::ModuleName> dependents = findReverseDependencies(ttv->definitionModuleName);

    // For every module, search for its referencing
    for (const auto& moduleName : dependents)
    {
        // Run the typechecker over the dependency modules
        checkStrict(moduleName);
        auto module = getModule(moduleName, /* forAutocomplete: */ true);
        if (!module)
            continue;

        auto sourceModule = frontend.getSourceModule(moduleName);
        if (!sourceModule)
            continue;

        if (property)
        {
            auto locations = findPropertyReferences(*sourceModule, property.value(), ty, module->astTypes);
            references.reserve(locations.size());
            for (auto location : locations)
                references.push_back(Reference{moduleName, location});
        }
        else
        {
            for (const auto [expr, referencedTy] : module->astTypes)
            {
                if (isSameTable(ty, Luau::follow(referencedTy)))
                    references.push_back(Reference{moduleName, expr->location});
            }
        }
    }

    // If its a property, include its original declaration location if not yet found
    if (property)
    {
        if (auto prop = lookupProp(ty, *property); prop && prop->location)
        {
            auto reference = Reference{ttv->definitionModuleName, prop->location.value()};
            if (!contains(references, reference))
                references.push_back(reference);
        }
    }

    return references;
}

// Find all references of an exported type
std::vector<Reference> WorkspaceFolder::findAllTypeReferences(const Luau::ModuleName& moduleName, const Luau::Name& typeName)
{
    std::vector<Reference> result;

    // Handle the module the type is declared in
    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return {};

    auto references = findTypeReferences(*sourceModule, typeName, std::nullopt);
    result.reserve(references.size() + 1);
    for (auto& location : references)
        result.emplace_back(Reference{moduleName, location});

    // Find the actual declaration location
    checkStrict(moduleName);
    auto module = getModule(moduleName, /* forAutocomplete: */ true);
    if (!module)
        return {};

    if (auto location = module->getModuleScope()->typeAliasNameLocations.find(typeName);
        location != module->getModuleScope()->typeAliasNameLocations.end())
        result.emplace_back(Reference{moduleName, location->second});

    // Find all cross-module references
    auto reverseDependencies = findReverseDependencies(moduleName);
    for (const auto& dependencyModuleName : reverseDependencies)
    {
        // Handle the imported module separately
        if (dependencyModuleName == moduleName)
            continue;

        // Run the typechecker over the dependency module
        checkStrict(dependencyModuleName);
        auto sourceModule = frontend.getSourceModule(dependencyModuleName);
        auto module = getModule(dependencyModuleName, /* forAutocomplete: */ true);
        if (sourceModule)
        {
            // Find the import name used
            Luau::Name importName;
            for (const auto& [name, mod] : module->getModuleScope()->importedModules)
            {
                if (mod == moduleName)
                {
                    importName = name;
                    break;
                }
            }

            if (importName.empty())
                continue;

            auto references = findTypeReferences(*sourceModule, typeName, importName);
            result.reserve(result.size() + references.size());
            for (auto& location : references)
                result.emplace_back(Reference{dependencyModuleName, location});
        }
    }

    return result;
}

static std::vector<lsp::Location> processReferences(WorkspaceFileResolver& fileResolver, const std::vector<Reference>& references)
{
    std::vector<lsp::Location> result{};

    for (const auto& reference : references)
    {
        if (auto refTextDocument = fileResolver.getTextDocumentFromModuleName(reference.moduleName))
        {
            result.emplace_back(lsp::Location{refTextDocument->uri(),
                {refTextDocument->convertPosition(reference.location.begin), refTextDocument->convertPosition(reference.location.end)}});
        }
        else
        {
            if (auto filePath = fileResolver.platform->resolveToRealPath(reference.moduleName))
            {
                if (auto source = fileResolver.readSource(reference.moduleName))
                {
                    auto refTextDocument = TextDocument{Uri::file(*filePath), "luau", 0, source->source};
                    result.emplace_back(lsp::Location{refTextDocument.uri(),
                        {refTextDocument.convertPosition(reference.location.begin), refTextDocument.convertPosition(reference.location.end)}});
                }
            }
        }
    }

    return result;
}

// Determines whether the name matches a type reference in one of the provided generics
// If so, we find the usages inside of that node
static bool handleIfTypeReferenceByName(Luau::AstNode* node, Luau::AstArray<Luau::AstGenericType> generics,
    Luau::AstArray<Luau::AstGenericTypePack> genericPacks, Luau::AstName name, std::vector<lsp::Location>& result, const TextDocument* textDocument)
{
    if (!isTypeReference(name, generics, genericPacks))
        return false;

    auto locations = findTypeParameterUsages(*node, name);
    result.reserve(result.size() + locations.size());
    for (auto& location : locations)
        result.emplace_back(
            lsp::Location{textDocument->uri(), {textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)}});

    return true;
}

// Determines whether the name matches a type reference in one of the provided generics
// If so, we find the usages inside of that node
static bool handleIfTypeReferenceByPosition(Luau::AstNode* node, Luau::AstArray<Luau::AstGenericType> generics,
    Luau::AstArray<Luau::AstGenericTypePack> genericPacks, Luau::Position position, std::vector<lsp::Location>& result,
    const TextDocument* textDocument)
{
    auto name = findTypeReferenceName(position, generics, genericPacks);
    if (!name)
        return false;

    auto locations = findTypeParameterUsages(*node, name.value());
    result.reserve(result.size() + locations.size());

    for (auto& location : locations)
        result.emplace_back(
            lsp::Location{textDocument->uri(), {textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)}});

    return true;
}

lsp::ReferenceResult WorkspaceFolder::references(const lsp::ReferenceParams& params)
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

    std::vector<lsp::Location> result{};

    if (symbol)
    {
        // Search for usages of a local or global symbol
        // TODO: what if this symbol is returned! need to handle that so we can find cross-file references
        auto references = findSymbolReferences(*sourceModule, symbol);

        result.reserve(references.size());
        for (auto& location : references)
        {
            result.emplace_back(
                lsp::Location{params.textDocument.uri, {textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)}});
        }

        return result;
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
                auto references = findAllReferences(parentTy, indexName->index.value);
                return processReferences(fileResolver, references);
            }
        }
        else if (auto constantString = expr->as<Luau::AstExprConstantString>())
        {
            // Potentially a property defined inside of a table
            auto ancestry = Luau::findAstAncestryOfPosition(*sourceModule, position, /* includeTypes= */ false);
            if (ancestry.size() > 1)
            {
                auto parent = ancestry.at(ancestry.size() - 2);
                if (auto tbl = parent->as<Luau::AstExprTable>())
                {
                    auto possibleTableTy = module->astTypes.find(tbl);
                    if (possibleTableTy)
                    {
                        auto references =
                            findAllReferences(Luau::follow(*possibleTableTy), Luau::Name(constantString->value.data, constantString->value.size));
                        return processReferences(fileResolver, references);
                    }
                }
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
        if (handleIfTypeReferenceByPosition(typeDefinition, typeDefinition->generics, typeDefinition->genericPacks, position, result, textDocument))
        {
            return result;
        }

        if (typeDefinition->exported)
        {
            // Type may potentially be used in other files, so we need to handle this globally
            auto references = findAllTypeReferences(moduleName, typeDefinition->name.value);
            return processReferences(fileResolver, references);
        }
        else
        {
            // Include all usages of the type
            auto references = findTypeReferences(*sourceModule, typeDefinition->name.value, std::nullopt);
            result.reserve(references.size() + 1);
            for (auto& location : references)
                result.emplace_back(lsp::Location{
                    params.textDocument.uri, {textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)}});


            // Include the type definition
            result.emplace_back(lsp::Location{params.textDocument.uri, {textDocument->convertPosition(typeDefinition->nameLocation.begin),
                                                                           textDocument->convertPosition(typeDefinition->nameLocation.end)}});

            return result;
        }
    }
    else if (auto reference = node->as<Luau::AstTypeReference>())
    {
        if (auto prefix = reference->prefix)
        {
            auto requireInfo = findClosestAncestorModuleImport(*sourceModule, reference->prefix.value(), reference->prefixLocation->begin);
            if (!requireInfo)
                return std::nullopt;

            auto [requireSymbol, requireExpr] = requireInfo.value();

            if (reference->prefixLocation.value().containsClosed(position))
            {
                // TODO: what if this symbol is returned! need to handle that so we can find cross-file references

                auto references = findSymbolReferences(*sourceModule, requireSymbol);
                result.reserve(references.size());
                for (auto& location : references)
                {
                    result.emplace_back(lsp::Location{
                        params.textDocument.uri, {textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)}});
                }

                return result;
            }
            else
            {
                auto moduleInfo = frontend.moduleResolver.resolveModuleInfo(module->name, *requireExpr);
                if (!moduleInfo)
                    return std::nullopt;
                auto references = findAllTypeReferences(moduleInfo->name, reference->name.value);
                return processReferences(fileResolver, references);
            }
        }
        else
        {
            // This could potentially be a generic type parameter - so we want to find its references if so.
            auto ancestry = Luau::findAstAncestryOfPosition(*sourceModule, position, /* includeTypes= */ true);
            for (auto it = ancestry.rbegin(); it != ancestry.rend(); ++it)
            {
                if (auto typeAlias = (*it)->as<Luau::AstStatTypeAlias>())
                {
                    if (handleIfTypeReferenceByName(typeAlias, typeAlias->generics, typeAlias->genericPacks, reference->name, result, textDocument))
                        return result;
                    break;
                }
                else if (auto typeFunction = (*it)->as<Luau::AstTypeFunction>())
                {
                    if (handleIfTypeReferenceByName(
                            typeFunction, typeFunction->generics, typeFunction->genericPacks, reference->name, result, textDocument))
                        return result;
                }
                else if (auto func = (*it)->as<Luau::AstExprFunction>())
                {
                    if (handleIfTypeReferenceByName(func, func->generics, func->genericPacks, reference->name, result, textDocument))
                        return result;
                    break;
                }
                else if (!(*it)->asType())
                {
                    // No longer inside a type, so no point going further
                    break;
                }
            }

            auto references = findTypeReferences(*sourceModule, reference->name.value, std::nullopt);
            result.reserve(references.size() + 1);
            for (auto& location : references)
                result.emplace_back(lsp::Location{
                    params.textDocument.uri, {textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)}});

            // Find the actual declaration location
            auto scope = Luau::findScopeAtPosition(*module, position);
            while (scope)
            {
                if (auto location = scope->typeAliasNameLocations.find(reference->name.value); location != scope->typeAliasNameLocations.end())
                {
                    result.emplace_back(lsp::Location{params.textDocument.uri,
                        {textDocument->convertPosition(location->second.begin), textDocument->convertPosition(location->second.end)}});
                    break;
                }

                scope = scope->parent;
            }

            return result;
        }
    }
    else if (auto typeFunction = node->as<Luau::AstTypeFunction>())
    {
        if (handleIfTypeReferenceByPosition(typeFunction, typeFunction->generics, typeFunction->genericPacks, position, result, textDocument))
        {
            return result;
        }
    }
    else if (auto func = node->as<Luau::AstExprFunction>())
    {
        if (handleIfTypeReferenceByPosition(func, func->generics, func->genericPacks, position, result, textDocument))
        {
            return result;
        }
    }

    return std::nullopt;
}

lsp::ReferenceResult LanguageServer::references(const lsp::ReferenceParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->references(params);
}
