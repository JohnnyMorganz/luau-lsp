#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"

#include "Luau/AstQuery.h"
#include "LSP/LuauExt.hpp"

#include <algorithm>

struct LocationInformation
{
    std::optional<std::string> definitionModuleName;
    std::optional<Luau::Location> location;
    Luau::TypeId ty;
};

static std::optional<LocationInformation> findLocationForSymbol(
    const Luau::ModulePtr& module, const Luau::Position& position, const Luau::Symbol& symbol)
{
    auto scope = Luau::findScopeAtPosition(*module, position);
    auto ty = scope->lookup(symbol);
    if (!ty)
        return std::nullopt;
    ty = Luau::follow(*ty);
    return LocationInformation{Luau::getDefinitionModuleName(*ty), getLocation(*ty), *ty};
}

static std::optional<LocationInformation> findLocationForIndex(const Luau::ModulePtr& module, const Luau::AstExpr* base, const Luau::Name& name)
{
    auto baseTy = module->astTypes.find(base);
    if (!baseTy)
        return std::nullopt;
    auto baseTyFollowed = Luau::follow(*baseTy);
    auto propInformation = lookupProp(baseTyFollowed, name);
    if (!propInformation)
        return std::nullopt;

    auto [realBaseTy, prop] = *propInformation;
    auto location = prop.location ? prop.location : prop.typeLocation;

    if (!prop.readTy)
        return std::nullopt;

    return LocationInformation{Luau::getDefinitionModuleName(realBaseTy), location, *prop.readTy};
}

static std::optional<LocationInformation> findLocationForExpr(
    const Luau::ModulePtr& module, const Luau::AstExpr* expr, const Luau::Position& position)
{
    auto scope = Luau::findScopeAtPosition(*module, position);

    if (auto local = expr->as<Luau::AstExprLocal>())
        return findLocationForSymbol(module, position, local->local);
    else if (auto global = expr->as<Luau::AstExprGlobal>())
        return findLocationForSymbol(module, position, global->name);
    else if (auto indexname = expr->as<Luau::AstExprIndexName>())
        return findLocationForIndex(module, indexname->expr, indexname->index.value);
    else if (auto indexexpr = expr->as<Luau::AstExprIndexExpr>())
    {
        if (auto string = indexexpr->index->as<Luau::AstExprConstantString>())
            return findLocationForIndex(module, indexexpr->expr, std::string(string->value.data, string->value.size));
    }

    return std::nullopt;
}

lsp::DefinitionResult WorkspaceFolder::gotoDefinition(const lsp::DefinitionParams& params)
{
    lsp::DefinitionResult result{};

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());
    auto position = textDocument->convertPosition(params.position);

    // Run the type checker to ensure we are up to date
    checkStrict(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    // TODO: fix "forAutocomplete"
    auto module = getModule(moduleName, /* forAutocomplete: */ true);
    if (!sourceModule || !module)
        return result;

    auto binding = Luau::findBindingAtPosition(*module, *sourceModule, position);
    if (binding)
    {
        // If it points to a global definition (i.e. at pos 0,0), return nothing
        if (binding->location.begin == Luau::Position{0, 0} && binding->location.end == Luau::Position{0, 0})
            return result;

        // Follow through the binding reference if it is a function type
        // This is particularly useful for `local X = require(...)` where `X` is a function - we want the actual function definition
        // TODO: Can we get further references for other types?
        auto ftv = Luau::get<Luau::FunctionType>(Luau::follow(binding->typeId));
        if (ftv && ftv->definition && ftv->definition->definitionModuleName)
        {
            if (auto document = fileResolver.getOrCreateTextDocumentFromModuleName(ftv->definition->definitionModuleName.value()))
            {
                result.emplace_back(lsp::Location{document->uri(), lsp::Range{document->convertPosition(ftv->definition->originalNameLocation.begin),
                                                                       document->convertPosition(ftv->definition->originalNameLocation.end)}});
                return result;
            }
        }

        result.emplace_back(lsp::Location{params.textDocument.uri,
            lsp::Range{textDocument->convertPosition(binding->location.begin), textDocument->convertPosition(binding->location.end)}});
        return result;
    }

    auto node = findNodeOrTypeAtPosition(*sourceModule, position);
    if (!node)
        return result;

    if (auto expr = node->asExpr())
    {
        auto locationInformation = findLocationForExpr(module, expr, position);
        if (locationInformation)
        {
            auto [definitionModuleName, location, _] = *locationInformation;
            if (location)
            {
                if (definitionModuleName)
                {
                    if (auto uri = platform->resolveToRealPath(*definitionModuleName))
                    {
                        if (auto document = fileResolver.getOrCreateTextDocumentFromModuleName(*definitionModuleName))
                        {
                            result.emplace_back(lsp::Location{
                                *uri, lsp::Range{document->convertPosition(location->begin), document->convertPosition(location->end)}});
                        }
                    }
                }
                else
                {
                    result.emplace_back(lsp::Location{params.textDocument.uri,
                        lsp::Range{textDocument->convertPosition(location->begin), textDocument->convertPosition(location->end)}});
                }
            }
        }
    }
    else if (auto reference = node->as<Luau::AstTypeReference>())
    {
        auto uri = params.textDocument.uri;
        TextDocumentPtr referenceTextDocument(textDocument);
        std::optional<Luau::Location> location = std::nullopt;

        auto scope = Luau::findScopeAtPosition(*module, position);
        if (!scope)
            return result;

        if (reference->prefix)
        {
            if (auto importedName = lookupImportedModule(*scope, reference->prefix.value().value))
            {
                auto importedModule = getModule(*importedName, /* forAutocomplete: */ true);
                if (!importedModule)
                    return result;

                const auto it = importedModule->exportedTypeBindings.find(reference->name.value);
                if (it == importedModule->exportedTypeBindings.end() || !it->second.definitionLocation)
                    return result;

                referenceTextDocument = fileResolver.getOrCreateTextDocumentFromModuleName(*importedName);
                location = *it->second.definitionLocation;
            }
            else
                return result;
        }
        else
        {
            location = lookupTypeLocation(*scope, reference->name.value);
        }

        if (!referenceTextDocument || !location)
            return result;

        result.emplace_back(lsp::Location{referenceTextDocument->uri(),
            lsp::Range{referenceTextDocument->convertPosition(location->begin), referenceTextDocument->convertPosition(location->end)}});
    }

    // Fallback: if no results found so far, we can try checking if this is within a require statement
    if (result.empty())
    {
        auto ancestry = Luau::findAstAncestryOfPosition(*sourceModule, position);
        if (ancestry.size() >= 2)
        {
            if (auto call = ancestry[ancestry.size() - 2]->as<Luau::AstExprCall>(); call && types::matchRequire(*call))
            {
                if (auto moduleInfo = frontend.moduleResolver.resolveModuleInfo(moduleName, *call))
                {
                    if (auto uri = platform->resolveToRealPath(moduleInfo->name))
                    {
                        result.emplace_back(lsp::Location{*uri, lsp::Range{{0, 0}, {0, 0}}});
                    }
                }
            }
        }
    }

    // Remove duplicate elements within the result
    // TODO: This is O(n^2). It shouldn't matter too much, since right now there will only be at most 2 elements.
    // But maybe we can remove the need for this in the first place?
    auto end = result.end();
    for (auto it = result.begin(); it != end; ++it)
        end = std::remove(it + 1, end, *it);

    result.erase(end, result.end());

    return result;
}

std::optional<lsp::Location> WorkspaceFolder::gotoTypeDefinition(const lsp::TypeDefinitionParams& params)
{
    // If its a binding, we should find its assigned type if possible, and then find the definition of that type
    // If its a type, then just find the definintion of that type (i.e. the type alias)

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());
    auto position = textDocument->convertPosition(params.position);

    // Run the type checker to ensure we are up to date
    checkStrict(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    // TODO: fix "forAutocomplete"
    auto module = getModule(moduleName, /* forAutocomplete: */ true);
    if (!sourceModule || !module)
        return std::nullopt;

    auto node = findNodeOrTypeAtPosition(*sourceModule, position);
    if (!node)
        return std::nullopt;

    auto findTypeLocation = [this, textDocument, &module, &position](Luau::AstType* type) -> std::optional<lsp::Location>
    {
        // TODO: should we only handle references here? what if its an actual type
        if (auto reference = type->as<Luau::AstTypeReference>())
        {
            TextDocumentPtr referenceTextDocument(textDocument);
            std::optional<Luau::Location> location = std::nullopt;

            auto scope = Luau::findScopeAtPosition(*module, position);
            if (!scope)
                return std::nullopt;

            if (reference->prefix)
            {
                if (auto importedName = lookupImportedModule(*scope, reference->prefix.value().value))
                {
                    auto importedModule = getModule(*importedName, /* forAutocomplete: */ true);
                    if (!importedModule)
                        return std::nullopt;

                    const auto it = importedModule->exportedTypeBindings.find(reference->name.value);
                    if (it == importedModule->exportedTypeBindings.end() || !it->second.definitionLocation)
                        return std::nullopt;

                    referenceTextDocument = fileResolver.getOrCreateTextDocumentFromModuleName(*importedName);
                    location = *it->second.definitionLocation;
                }
                else
                    return std::nullopt;
            }
            else
            {
                location = lookupTypeLocation(*scope, reference->name.value);
            }

            if (!referenceTextDocument || !location)
                return std::nullopt;

            return lsp::Location{referenceTextDocument->uri(),
                lsp::Range{referenceTextDocument->convertPosition(location->begin), referenceTextDocument->convertPosition(location->end)}};
        }
        return std::nullopt;
    };

    if (auto type = node->asType())
    {
        return findTypeLocation(type);
    }
    else if (auto typeAlias = node->as<Luau::AstStatTypeAlias>())
    {
        return findTypeLocation(typeAlias->type);
    }
    else if (auto expr = node->asExpr())
    {
        if (auto ty = module->astTypes.find(expr))
        {
            auto followedTy = Luau::follow(*ty);
            auto definitionModuleName = Luau::getDefinitionModuleName(followedTy);
            auto location = getLocation(followedTy);

            if (definitionModuleName && location)
            {
                auto document = fileResolver.getOrCreateTextDocumentFromModuleName(*definitionModuleName);
                if (document)
                    return lsp::Location{
                        document->uri(), lsp::Range{document->convertPosition(location->begin), document->convertPosition(location->end)}};
            }
        }
    }

    return std::nullopt;
}

lsp::DefinitionResult LanguageServer::gotoDefinition(const lsp::DefinitionParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->gotoDefinition(params);
}

std::optional<lsp::Location> LanguageServer::gotoTypeDefinition(const lsp::TypeDefinitionParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->gotoTypeDefinition(params);
}
