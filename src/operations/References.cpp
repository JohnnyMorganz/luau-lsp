#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"

#include "Luau/AstQuery.h"
#include "LSP/LuauExt.hpp"

static bool isSameTable(const Luau::TypeId a, const Luau::TypeId b)
{
    if (a == b)
        return true;

    // TODO: in some cases, the table in the first module doesnt point to the same ty as the table in the second
    // for example, in the first module (the one being required), the table may be unsealed
    // we check for location equality in these cases
    if (auto ttv1 = Luau::get<Luau::TableType>(a))
        if (auto ttv2 = Luau::get<Luau::TableType>(b))
            return !ttv1->definitionModuleName.empty() && ttv1->definitionModuleName == ttv2->definitionModuleName &&
                   ttv1->definitionLocation == ttv2->definitionLocation;

    return false;
}

static bool isSameFunction(const Luau::TypeId a, const Luau::TypeId b)
{
    if (a == b)
        return true;

    // Two FunctionTypes may be different if one is local to the module and the other is part of the exported interface.
    // We check for location equality to handle this case
    if (auto ftv1 = Luau::get<Luau::FunctionType>(a); ftv1 && ftv1->definition && ftv1->definition->definitionModuleName)
        if (auto ftv2 = Luau::get<Luau::FunctionType>(b); ftv2 && ftv2->definition && ftv2->definition->definitionModuleName)
            return ftv1->definition->definitionModuleName == ftv2->definition->definitionModuleName &&
                   ftv1->definition->definitionLocation == ftv2->definition->definitionLocation;
    return false;
}

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

std::vector<Reference> WorkspaceFolder::findAllFunctionReferences(Luau::TypeId ty)
{
    ty = Luau::follow(ty);
    auto ftv = Luau::get<Luau::FunctionType>(ty);
    if (!ftv)
        return {};

    if (!ftv->definition || !ftv->definition->definitionModuleName || ftv->definition->definitionModuleName->empty())
        return {};

    std::vector<Reference> references;
    std::vector<Luau::ModuleName> dependents = findReverseDependencies(ftv->definition->definitionModuleName.value());

    for (const auto& moduleName : dependents)
    {
        checkStrict(moduleName);
        auto module = getModule(moduleName, /* forAutocomplete: */ true);
        if (!module)
            continue;

        for (const auto [expr, referencedTy] : module->astTypes)
        {
            // Skip the actual function definition
            if (expr->is<Luau::AstExprFunction>())
                continue;
            if (isSameFunction(ty, Luau::follow(referencedTy)))
                references.emplace_back(Reference{moduleName, expr->location});
        }
    }

    // Include original definition location
    references.emplace_back(Reference{ftv->definition->definitionModuleName.value(), ftv->definition->originalNameLocation});

    return references;
}

// Find all references across all files for the usage of TableType, or a property on a TableType
std::vector<Reference> WorkspaceFolder::findAllTableReferences(Luau::TypeId ty, std::optional<Luau::Name> property)
{
    ty = Luau::follow(ty);
    auto ttv = Luau::get<Luau::TableType>(ty);

    if (!ttv)
        return {};

    if (ttv->definitionModuleName.empty())
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

        for (const auto [expr, referencedTy] : module->astTypes)
        {
            // If we are looking for a property specifically,
            // then only look for LuauAstExprIndexName
            if (property)
            {
                if (auto indexName = expr->as<Luau::AstExprIndexName>(); indexName && indexName->index.value == property.value())
                {
                    auto possibleParentTy = module->astTypes.find(indexName->expr);
                    if (possibleParentTy && isSameTable(ty, Luau::follow(*possibleParentTy)))
                        references.push_back(Reference{moduleName, indexName->indexLocation});
                }
                else if (auto table = expr->as<Luau::AstExprTable>(); table && isSameTable(ty, Luau::follow(referencedTy)))
                {
                    for (const auto& item : table->items)
                    {
                        if (item.key)
                        {
                            if (auto propName = item.key->as<Luau::AstExprConstantString>())
                            {
                                if (propName->value.data == property.value())
                                    references.push_back(Reference{moduleName, item.key->location});
                            }
                        }
                    }
                }
            }
            else
            {
                if (isSameTable(ty, Luau::follow(referencedTy)))
                    references.push_back(Reference{moduleName, expr->location});
            }
        }
    }

    // If its a property, include its original declaration location if not yet found
    if (property)
    {
        if (auto propInformation = lookupProp(ty, *property))
        {
            auto [baseTy, prop] = propInformation.value();
            if (prop.location)
            {
                auto reference = Reference{Luau::getDefinitionModuleName(baseTy).value_or(ttv->definitionModuleName), prop.location.value()};
                if (!contains(references, reference))
                    references.push_back(reference);
            }
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
// If so, we find the usages inside of that node
bool handleIfTypeReferenceByName(Luau::AstNode* node, Luau::AstArray<Luau::AstGenericType> generics,
    Luau::AstArray<Luau::AstGenericTypePack> genericPacks, Luau::AstName name, std::vector<lsp::Location>& result, const TextDocument* textDocument)
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

    for (auto& location : visitor.result)
        result.emplace_back(
            lsp::Location{textDocument->uri(), {textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)}});

    return true;
}

// Determines whether the name matches a type reference in one of the provided generics
// If so, we find the usages inside of that node
bool handleIfTypeReferenceByPosition(Luau::AstNode* node, Luau::AstArray<Luau::AstGenericType> generics,
    Luau::AstArray<Luau::AstGenericTypePack> genericPacks, Luau::Position position, std::vector<lsp::Location>& result,
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

    for (auto& location : visitor.result)
        result.emplace_back(
            lsp::Location{textDocument->uri(), {textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)}});

    return true;
}

static bool typeIsReturned(Luau::TypePackId returnType, Luau::TypeId ty)
{
    if (!returnType)
        return false;

    ty = Luau::follow(ty);

    for (const auto& part : returnType)
    {
        auto otherTy = Luau::follow(part);
        if (isSameTable(ty, otherTy) || isSameFunction(ty, otherTy))
            return true;
    }

    return false;
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

    auto module = getModule(moduleName, /* forAutocomplete: */ true);

    if (symbol)
    {
        // Check if symbol was a returned symbol
        // We currently only handle returned functions and tables
        // TODO: maybe we can cover more cases if we generalise this to be AST-based rather than type based?
        // (we could track usages of local vars, cross-module track usages of X in `local X = require()`)
        auto scope = Luau::findScopeAtPosition(*module, position);
        if (auto ty = scope->lookup(symbol))
        {
            auto followedTy = Luau::follow(*ty);
            if (typeIsReturned(module->returnType, followedTy))
            {
                std::vector<Reference> references;
                if (Luau::get<Luau::FunctionType>(followedTy))
                    references = findAllFunctionReferences(followedTy);
                else if (Luau::get<Luau::TableType>(followedTy))
                    references = findAllTableReferences(followedTy);

                return processReferences(fileResolver, references);
            }
        }

        // Search for usages of a local or global symbol
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
    if (auto expr = exprOrLocal.getExpr())
    {
        if (auto indexName = expr->as<Luau::AstExprIndexName>())
        {
            auto possibleParentTy = module->astTypes.find(indexName->expr);
            if (possibleParentTy)
            {
                auto parentTy = Luau::follow(*possibleParentTy);
                auto references = findAllTableReferences(parentTy, indexName->index.value);
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
                        auto references = findAllTableReferences(
                            Luau::follow(*possibleTableTy), Luau::Name(constantString->value.data, constantString->value.size));
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
            ;
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
            if (auto importedModuleName = module->getModuleScope()->importedModules.find(prefix.value().value);
                importedModuleName != module->getModuleScope()->importedModules.end())
            {
                auto references = findAllTypeReferences(importedModuleName->second, reference->name.value);
                return processReferences(fileResolver, references);
            }

            return std::nullopt;
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
