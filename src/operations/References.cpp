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

// Find all references across all files for the usage of TableType, or a property on a TableType
std::optional<std::vector<Reference>> WorkspaceFolder::findAllReferences(Luau::TypeId ty, std::optional<Luau::Name> property)
{
    ty = Luau::follow(ty);
    auto ttv = Luau::get<Luau::TableType>(ty);

    if (!ttv)
        return std::nullopt;

    if (ttv->definitionModuleName.empty())
        return std::nullopt;

    std::vector<Reference> references;

    // Find all reverse dependencies of the top-level module
    // TODO: this function is quite expensive as it requires a BFS
    // TODO: this comes from `markDirty`
    std::vector<Luau::ModuleName> dependents{};
    std::unordered_map<Luau::ModuleName, std::vector<Luau::ModuleName>> reverseDeps;
    for (const auto& module : frontend.sourceNodes)
    {
        for (const auto& dep : module.second.requireSet)
            reverseDeps[dep].push_back(module.first);
    }
    std::vector<Luau::ModuleName> queue{ttv->definitionModuleName};
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

    // For every module, search for its referencing
    for (const auto& moduleName : dependents)
    {
        auto module = frontend.moduleResolverForAutocomplete.getModule(moduleName);
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
                        references.push_back(Reference{moduleName, indexName->indexLocation, expr});
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
                                    references.push_back(Reference{moduleName, item.key->location, item.key});
                            }
                        }
                    }
                }
            }
            else
            {
                if (isSameTable(ty, Luau::follow(referencedTy)))
                    references.push_back(Reference{moduleName, expr->location, expr});
            }
        }
    }

    return references;
}


lsp::ReferenceResult WorkspaceFolder::references(const lsp::ReferenceParams& params)
{
    // TODO: we do not yet support finding type references
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());

    auto position = textDocument->convertPosition(params.position);

    // Run the type checker to ensure we are up to date
    // We check for autocomplete here since autocomplete has stricter types
    frontend.check(moduleName, Luau::FrontendOptions{/* retainFullTypeGraphs: */ true, /* forAutocomplete: */ true});

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return std::nullopt;

    auto exprOrLocal = Luau::findExprOrLocalAtPosition(*sourceModule, position);
    Luau::Symbol localSymbol;
    if (exprOrLocal.getLocal())
        localSymbol = exprOrLocal.getLocal();
    else if (auto exprLocal = exprOrLocal.getExpr()->as<Luau::AstExprLocal>())
        localSymbol = exprLocal->local;


    std::vector<lsp::Location> result{};

    if (localSymbol)
    {
        // Search for a local binding
        // TODO: what if this local binding is returned! need to handle that so we can find cross-file references
        auto references = findSymbolReferences(*sourceModule, localSymbol);

        result.reserve(references.size());
        for (auto& location : references)
        {
            result.emplace_back(
                lsp::Location{params.textDocument.uri, {textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)}});
        }

        return result;
    }

    // Search for a property
    auto module = frontend.moduleResolverForAutocomplete.getModule(moduleName);
    if (auto indexName = exprOrLocal.getExpr()->as<Luau::AstExprIndexName>())
    {
        auto possibleParentTy = module->astTypes.find(indexName->expr);
        if (possibleParentTy)
        {
            auto parentTy = Luau::follow(*possibleParentTy);
            auto references = findAllReferences(parentTy, indexName->index.value);
            if (references)
            {
                for (const auto& reference : *references)
                {
                    if (auto refTextDocument = fileResolver.getTextDocumentFromModuleName(reference.moduleName))
                    {
                        result.emplace_back(lsp::Location{refTextDocument->uri(),
                            {refTextDocument->convertPosition(reference.location.begin), refTextDocument->convertPosition(reference.location.end)}});
                    }
                    else
                    {
                        if (auto filePath = fileResolver.resolveToRealPath(reference.moduleName))
                        {
                            if (auto source = fileResolver.readSource(reference.moduleName))
                            {
                                auto refTextDocument = TextDocument{Uri::file(*filePath), "luau", 0, source->source};
                                result.emplace_back(
                                    lsp::Location{refTextDocument.uri(), {refTextDocument.convertPosition(reference.location.begin),
                                                                             refTextDocument.convertPosition(reference.location.end)}});
                            }
                        }
                    }
                }

                return result;
            }
        }
    }

    return std::nullopt;
}

lsp::ReferenceResult LanguageServer::references(const lsp::ReferenceParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->references(params);
}