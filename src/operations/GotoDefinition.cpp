#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"

#include "Luau/AstQuery.h"
#include "LSP/LuauExt.hpp"

#include <algorithm>

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
    auto module = frontend.moduleResolverForAutocomplete.getModule(moduleName);
    if (!sourceModule || !module)
        return result;

    if (auto platformResult = platform->handleGotoDefinition(*textDocument, *sourceModule, position))
        return platformResult.value();

    auto binding = Luau::findBindingAtPosition(*module, *sourceModule, position);
    if (binding)
    {
        // If it points to a global definition (i.e. at pos 0,0), return nothing
        if (binding->location.begin == Luau::Position{0, 0} && binding->location.end == Luau::Position{0, 0})
            return result;

        // TODO: can we maybe get further references if it points to something like `local X = require(...)`?
        result.emplace_back(lsp::Location{params.textDocument.uri,
            lsp::Range{textDocument->convertPosition(binding->location.begin), textDocument->convertPosition(binding->location.end)}});
    }

    auto node = findNodeOrTypeAtPosition(*sourceModule, position);
    if (!node)
        return result;

    if (auto expr = node->asExpr())
    {
        std::optional<Luau::ModuleName> definitionModuleName = std::nullopt;
        std::optional<Luau::Location> location = std::nullopt;

        if (auto lvalue = Luau::tryGetLValue(*expr))
        {
            const Luau::LValue* current = &*lvalue;
            std::vector<std::string> keys{}; // keys in reverse order
            while (auto field = Luau::get<Luau::Field>(*current))
            {
                keys.push_back(field->key);
                current = Luau::baseof(*current);
            }

            const auto* symbol = Luau::get<Luau::Symbol>(*current);
            auto scope = Luau::findScopeAtPosition(*module, position);
            if (!scope)
                return result;

            auto baseType = scope->lookup(*symbol);
            if (!baseType)
                return result;
            baseType = Luau::follow(*baseType);

            std::vector<Luau::Property> properties{};
            for (auto it = keys.rbegin(); it != keys.rend(); ++it)
            {
                auto base = properties.empty() ? *baseType : Luau::follow(properties.back().type());
                auto prop = lookupProp(base, *it);
                if (!prop)
                    return result;
                properties.push_back(*prop);
            }

            for (auto it = properties.rbegin(); it != properties.rend(); ++it)
            {
                if (!location && it->location)
                    location = it->location;
                if (!definitionModuleName)
                    definitionModuleName = Luau::getDefinitionModuleName(Luau::follow(it->type()));
            }

            if (!definitionModuleName)
                definitionModuleName = Luau::getDefinitionModuleName(*baseType);

            if (!location)
                location = getLocation(*baseType);
        }

        if (location)
        {
            if (definitionModuleName)
            {
                if (auto file = fileResolver.resolveToRealPath(*definitionModuleName))
                {
                    auto document = fileResolver.getTextDocumentFromModuleName(*definitionModuleName);
                    auto uri = document ? document->uri() : Uri::file(*file);
                    result.emplace_back(lsp::Location{uri, lsp::Range{toUTF16(document, location->begin), toUTF16(document, location->end)}});
                }
            }
            else
            {
                result.emplace_back(lsp::Location{params.textDocument.uri,
                    lsp::Range{textDocument->convertPosition(location->begin), textDocument->convertPosition(location->end)}});
            }
        }
    }
    else if (auto reference = node->as<Luau::AstTypeReference>())
    {
        auto uri = params.textDocument.uri;
        TextDocumentPtr referenceTextDocument(textDocument);

        auto scope = Luau::findScopeAtPosition(*module, position);
        if (!scope)
            return result;

        if (reference->prefix)
        {
            if (auto importedName = lookupImportedModule(*scope, reference->prefix.value().value))
            {
                auto fileName = fileResolver.resolveToRealPath(*importedName);
                if (!fileName)
                    return result;
                uri = Uri::file(*fileName);

                // TODO: fix "forAutocomplete"
                if (auto importedModule = frontend.moduleResolverForAutocomplete.getModule(*importedName);
                    importedModule && importedModule->hasModuleScope())
                    scope = importedModule->getModuleScope();
                else
                    return result;

                referenceTextDocument = fileResolver.getOrCreateTextDocumentFromModuleName(*importedName);
                if (!referenceTextDocument)
                    return result;
            }
            else
                return result;
        }

        auto location = lookupTypeLocation(*scope, reference->name.value);
        if (!location)
            return result;

        result.emplace_back(lsp::Location{
            uri, lsp::Range{referenceTextDocument->convertPosition(location->begin), referenceTextDocument->convertPosition(location->end)}});
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
    auto module = frontend.moduleResolverForAutocomplete.getModule(moduleName);
    if (!sourceModule || !module)
        return std::nullopt;

    if (auto result = platform->handleGotoTypeDefinition(*textDocument, *sourceModule, position))
        return result;

    auto node = findNodeOrTypeAtPosition(*sourceModule, position);
    if (!node)
        return std::nullopt;

    auto findTypeLocation = [this, textDocument, &module, &position, &params](Luau::AstType* type) -> std::optional<lsp::Location>
    {
        // TODO: should we only handle references here? what if its an actual type
        if (auto reference = type->as<Luau::AstTypeReference>())
        {
            auto uri = params.textDocument.uri;
            TextDocumentPtr referenceTextDocument(textDocument);

            auto scope = Luau::findScopeAtPosition(*module, position);
            if (!scope)
                return std::nullopt;

            if (reference->prefix)
            {
                if (auto importedName = lookupImportedModule(*scope, reference->prefix.value().value))
                {
                    auto fileName = fileResolver.resolveToRealPath(*importedName);
                    if (!fileName)
                        return std::nullopt;
                    uri = Uri::file(*fileName);

                    // TODO: fix "forAutocomplete"
                    if (auto importedModule = frontend.moduleResolverForAutocomplete.getModule(*importedName);
                        importedModule && importedModule->hasModuleScope())
                        scope = importedModule->getModuleScope();
                    else
                        return std::nullopt;

                    referenceTextDocument = fileResolver.getOrCreateTextDocumentFromModuleName(*importedName);
                    if (!referenceTextDocument)
                        return std::nullopt;
                }
                else
                    return std::nullopt;
            }

            auto location = lookupTypeLocation(*scope, reference->name.value);
            if (!location)
                return std::nullopt;

            return lsp::Location{
                uri, lsp::Range{referenceTextDocument->convertPosition(location->begin), referenceTextDocument->convertPosition(location->end)}};
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
    else if (auto localExpr = node->as<Luau::AstExprLocal>())
    {
        if (auto local = localExpr->local)
        {
            if (auto annotation = local->annotation)
            {
                return findTypeLocation(annotation);
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
