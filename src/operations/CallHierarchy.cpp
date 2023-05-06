#include <LSP/Workspace.hpp>
#include <LSP/LuauExt.hpp>
#include <Luau/AstQuery.h>

std::vector<lsp::CallHierarchyItem> WorkspaceFolder::prepareCallHierarchy(const lsp::CallHierarchyPrepareParams& params)
{
    // TODO: this is largely based off goto definition, maybe DRY?

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());
    auto position = textDocument->convertPosition(params.position);

    // Run the type checker to ensure we are up to date
    if (frontend.isDirty(moduleName))
    {
        // TODO: expressiveTypes - remove "forAutocomplete" once the types have been fixed
        Luau::FrontendOptions frontendOpts{true, true};
        frontend.check(moduleName, frontendOpts);
    }

    auto sourceModule = frontend.getSourceModule(moduleName);
    auto module = frontend.moduleResolverForAutocomplete.getModule(moduleName);
    if (!sourceModule || !module)
        return {};


    auto scope = Luau::findScopeAtPosition(*module, position);
    if (!scope)
        return {};

    auto exprOrLocal = Luau::findExprOrLocalAtPosition(*sourceModule, position);
    if (!exprOrLocal.getLocal() && !exprOrLocal.getExpr())
        return {};

    Luau::TypeId ty = nullptr;

    if (auto local = exprOrLocal.getLocal())
    {
        ty = scope->lookup(local).value_or(nullptr);
    }
    else if (auto lvalue = Luau::tryGetLValue(*exprOrLocal.getExpr()))
    {
        const Luau::LValue* current = &*lvalue;
        std::vector<std::string> keys{}; // keys in reverse order
        while (auto field = Luau::get<Luau::Field>(*current))
        {
            keys.push_back(field->key);
            current = Luau::baseof(*current);
        }

        const auto* symbol = Luau::get<Luau::Symbol>(*current);

        if (auto baseType = scope->lookup(*symbol))
            ty = Luau::follow(*baseType);
        else
            return {};

        for (auto it = keys.rbegin(); it != keys.rend(); ++it)
        {
            auto prop = lookupProp(ty, *it);
            if (!prop)
                return {};
            ty = Luau::follow(prop->type);
        }
    }

    if (!ty)
        return {};

    if (auto ftv = Luau::get<Luau::FunctionType>(ty); ftv && ftv->definition && ftv->definition->definitionModuleName)
    {
        auto itemModuleName = ftv->definition->definitionModuleName.value();
        lsp::CallHierarchyItem item{};

        if (auto name = exprOrLocal.getName())
            item.name = name.value().value;
        else
        {
            // TODO
        }


        item.kind = lsp::SymbolKind::Function;

        if (auto refTextDocument = fileResolver.getTextDocumentFromModuleName(itemModuleName))
        {
            item.uri = refTextDocument->uri();
            item.range = {refTextDocument->convertPosition(ftv->definition->definitionLocation.begin),
                refTextDocument->convertPosition(ftv->definition->definitionLocation.end)};
            item.selectionRange = {refTextDocument->convertPosition(ftv->definition->originalNameLocation.begin),
                refTextDocument->convertPosition(ftv->definition->originalNameLocation.end)};
        }
        else if (auto filePath = fileResolver.resolveToRealPath(itemModuleName))
        {
            if (auto source = fileResolver.readSource(itemModuleName))
            {
                auto refTextDocument = TextDocument{Uri::file(*filePath), "luau", 0, source->source};
                item.uri = refTextDocument.uri();
                item.range = {refTextDocument.convertPosition(ftv->definition->definitionLocation.begin),
                    refTextDocument.convertPosition(ftv->definition->definitionLocation.end)};
                item.selectionRange = {refTextDocument.convertPosition(ftv->definition->originalNameLocation.begin),
                    refTextDocument.convertPosition(ftv->definition->originalNameLocation.end)};
            }
            else
                return {};
        }
        else
            return {};

        return {item};
    }
    else
        return {};
}

std::vector<lsp::CallHierarchyIncomingCall> WorkspaceFolder::callHierarchyIncomingCalls(const lsp::CallHierarchyIncomingCallsParams& params)
{
    // TODO
    return {};
}
std::vector<lsp::CallHierarchyOutgoingCall> WorkspaceFolder::callHierarchyOutgoingCalls(const lsp::CallHierarchyOutgoingCallsParams& params)
{
    // TODO
    return {};
}