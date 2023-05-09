#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"

#include "Luau/AstQuery.h"
#include "Luau/ToString.h"
#include "LSP/LuauExt.hpp"
#include "LSP/DocumentationParser.hpp"

struct DocumentationLocation
{
    Luau::ModuleName moduleName;
    Luau::Location location;
};

std::optional<lsp::Hover> WorkspaceFolder::hover(const lsp::HoverParams& params)
{
    auto config = client->getConfiguration(rootUri);

    if (!config.hover.enabled)
        return std::nullopt;

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());

    auto position = textDocument->convertPosition(params.position);

    // Run the type checker to ensure we are up to date
    // TODO: expressiveTypes - remove "forAutocomplete" once the types have been fixed
    Luau::FrontendOptions frontendOpts{/* retainFullTypeGraphs: */ true, /* forAutocomplete: */ config.hover.strictDatamodelTypes};
    frontend.check(moduleName, frontendOpts);

    auto sourceModule = frontend.getSourceModule(moduleName);
    auto module = config.hover.strictDatamodelTypes ? frontend.moduleResolverForAutocomplete.getModule(moduleName)
                                                    : frontend.moduleResolver.getModule(moduleName);
    if (!sourceModule)
        return std::nullopt;

    auto exprOrLocal = Luau::findExprOrLocalAtPosition(*sourceModule, position);
    auto node = findNodeOrTypeAtPosition(*sourceModule, position);
    auto scope = Luau::findScopeAtPosition(*module, position);
    if (!node || !scope)
        return std::nullopt;

    std::string typeName;
    std::optional<Luau::TypeId> type = std::nullopt;
    std::optional<std::string> documentationSymbol = getDocumentationSymbolAtPosition(*sourceModule, *module, position);
    std::optional<DocumentationLocation> documentationLocation = std::nullopt;

    if (auto ref = node->as<Luau::AstTypeReference>())
    {
        std::optional<Luau::TypeFun> typeFun;
        if (ref->prefix)
        {
            typeName = std::string(ref->prefix->value) + "." + ref->name.value;
            typeFun = scope->lookupImportedType(ref->prefix->value, ref->name.value);
        }
        else
        {
            typeName = ref->name.value;
            typeFun = scope->lookupType(ref->name.value);
        }
        if (!typeFun)
            return std::nullopt;
        type = typeFun->type;
    }
    else if (auto alias = node->as<Luau::AstStatTypeAlias>())
    {
        typeName = alias->name.value;
        auto typeFun = scope->lookupType(typeName);
        if (!typeFun)
            return std::nullopt;
        type = typeFun->type;
    }
    else if (auto astType = node->asType())
    {
        if (auto ty = module->astResolvedTypes.find(astType))
        {
            type = *ty;
        }
    }
    else if (auto local = exprOrLocal.getLocal()) // TODO: can we just use node here instead of also calling exprOrLocal?
    {
        type = scope->lookup(local);
        documentationLocation = {moduleName, local->location};
    }
    else if (auto expr = exprOrLocal.getExpr())
    {
        // Special case, we want to check if there is a parent in the ancestry, and if it is an AstTable
        // If so, and we are hovering over a prop, we want to give type info for the assigned expression to the prop
        // rather than just "string"
        auto ancestry = Luau::findAstAncestryOfPosition(*sourceModule, position);
        if (ancestry.size() >= 2 && ancestry.at(ancestry.size() - 2)->is<Luau::AstExprTable>())
        {
            auto parent = ancestry.at(ancestry.size() - 2)->as<Luau::AstExprTable>();
            for (const auto& [kind, key, value] : parent->items)
            {
                if (key && key->location.contains(position))
                {
                    // Return type type of the value
                    if (auto it = module->astTypes.find(value))
                    {
                        type = *it;
                    }
                    break;
                }
            }
        }

        // Handle table properties (so that we can get documentation info)
        if (auto index = expr->as<Luau::AstExprIndexName>())
        {
            if (auto parentIt = module->astTypes.find(index->expr))
            {
                auto parentType = Luau::follow(*parentIt);
                auto indexName = index->index.value;
                auto prop = lookupProp(parentType, indexName);
                if (prop)
                {
                    type = prop->type();
                    if (auto definitionModuleName = Luau::getDefinitionModuleName(parentType); definitionModuleName && prop->location)
                        documentationLocation = {definitionModuleName.value(), prop->location.value()};
                }
            }
        }

        // Handle local variables separately to retrieve documentation location info
        if (auto local = expr->as<Luau::AstExprLocal>(); !documentationLocation.has_value() && local && local->local)
        {
            documentationLocation = {moduleName, local->local->location};
        }

        if (!type)
        {
            if (auto it = module->astTypes.find(expr))
            {
                type = *it;
            }
            else if (auto global = expr->as<Luau::AstExprGlobal>())
            {
                type = scope->lookup(global->name);
            }
            else if (auto local = expr->as<Luau::AstExprLocal>())
            {
                type = scope->lookup(local->local);
            }
        }
    }

    if (!type)
        return std::nullopt;
    type = Luau::follow(*type);

    if (!documentationSymbol)
        documentationSymbol = type.value()->documentationSymbol;

    Luau::ToStringOptions opts;
    opts.exhaustive = true;
    opts.useLineBreaks = true;
    opts.functionTypeArguments = true;
    opts.hideNamedFunctionTypeParameters = false;
    opts.hideTableKind = !config.hover.showTableKinds;
    opts.scope = scope;
    std::string typeString = Luau::toString(*type, opts);

    // If we have a function and its corresponding name
    if (!typeName.empty())
    {
        typeString = codeBlock("lua", "type " + typeName + " = " + typeString);
    }
    else if (auto ftv = Luau::get<Luau::FunctionType>(*type))
    {
        types::NameOrExpr name = "";
        if (auto localName = exprOrLocal.getName())
            name = localName->value;
        else if (auto expr = exprOrLocal.getExpr())
            name = expr;

        types::ToStringNamedFunctionOpts funcOpts;
        funcOpts.hideTableKind = !config.hover.showTableKinds;
        funcOpts.multiline = config.hover.multilineFunctionDefinitions;
        typeString = codeBlock("lua", types::toStringNamedFunction(module, ftv, name, scope, funcOpts));
    }
    else if (exprOrLocal.getLocal() || node->as<Luau::AstExprLocal>())
    {
        std::string builder = "local ";
        if (auto name = exprOrLocal.getName())
            builder += name->value;
        else
            builder += Luau::getIdentifier(node->asExpr()).value;
        builder += ": " + typeString;
        typeString = codeBlock("lua", builder);
    }
    else if (auto global = node->as<Luau::AstExprGlobal>())
    {
        // TODO: should we indicate this is a global somehow?
        std::string builder = "type ";
        builder += global->name.value;
        builder += " = " + typeString;
        typeString = codeBlock("lua", builder);
    }
    else
    {
        typeString = codeBlock("lua", typeString);
    }

    if (std::optional<std::string> docs; documentationSymbol && (docs = printDocumentation(client->documentation, *documentationSymbol)) && docs)
    {
        typeString += "\n----------\n";
        typeString += *docs;
    }
    else if (auto documentation = getDocumentationForType(*type))
    {
        typeString += "\n----------\n";
        typeString += *documentation;
    }
    else if (documentationLocation)
    {
        typeString += "\n----------\n";
        typeString += printMoonwaveDocumentation(getComments(documentationLocation->moduleName, documentationLocation->location));
    }

    return lsp::Hover{{lsp::MarkupKind::Markdown, typeString}};
}

std::optional<lsp::Hover> LanguageServer::hover(const lsp::HoverParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->hover(params);
}