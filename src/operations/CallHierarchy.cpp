#include <LSP/Workspace.hpp>
#include <LSP/LuauExt.hpp>
#include <Luau/AstQuery.h>
#include <Luau/Transpiler.h>

struct FindAllCallsVisitor : public Luau::AstVisitor
{
    std::vector<const Luau::AstExprCall*> calls;

    explicit FindAllCallsVisitor() {}

    bool visit(class Luau::AstExprCall* node) override
    {
        calls.emplace_back(node);
        return true;
    }
};


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

        if (auto refTextDocument = fileResolver.getOrCreateTextDocumentFromModuleName(itemModuleName))
        {
            item.uri = refTextDocument->uri();
            item.range = {refTextDocument->convertPosition(ftv->definition->definitionLocation.begin),
                refTextDocument->convertPosition(ftv->definition->definitionLocation.end)};
            item.selectionRange = {refTextDocument->convertPosition(ftv->definition->originalNameLocation.begin),
                refTextDocument->convertPosition(ftv->definition->originalNameLocation.end)};
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
    auto moduleName = fileResolver.getModuleName(params.item.uri);

    // NOTE: the text document in this case may not necessarily be managed
    auto textDocument = fileResolver.getOrCreateTextDocumentFromModuleName(moduleName);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No text document available for " + params.item.uri.toString());
    auto position = textDocument->convertPosition(params.item.selectionRange.start);

    // Find the original function in the file
    auto sourceModule = frontend.getSourceModule(moduleName);
    auto module = frontend.moduleResolverForAutocomplete.getModule(moduleName);
    if (!sourceModule || !module)
        return {};
    auto node = Luau::findExprAtPosition(*sourceModule, position);
    if (!node || !node->is<Luau::AstExprFunction>())
        return {};

    auto func = node->as<Luau::AstExprFunction>();

    FindAllCallsVisitor visitor;
    func->visit(&visitor);

    if (visitor.calls.empty())
        return {};

    std::vector<lsp::CallHierarchyOutgoingCall> result;

    // Aggregate calls together based on FunctionType
    std::unordered_map<Luau::TypeId, std::vector<Luau::AstExpr*>> calls{};
    for (const auto& call : visitor.calls)
    {
        if (auto ty = module->astTypes.find(call->func))
        {
            auto followedTy = Luau::follow(*ty);
            if (!contains(calls, followedTy))
                calls.emplace(followedTy, std::vector<Luau::AstExpr*>{});
            calls.at(followedTy).emplace_back(call->func);
        }
    }

    // Convert calls into hierarchy items
    for (const auto& [ty, exprs] : calls)
    {
        if (auto ftv = Luau::get<Luau::FunctionType>(ty); ftv && ftv->definition && ftv->definition->definitionModuleName)
        {
            lsp::CallHierarchyItem item{};

            // Find the first "normal" call to determine the name of the item
            for (const auto& expr : exprs)
            {
                if (auto name = expr->as<Luau::AstExprLocal>())
                {
                    item.name = name->local->name.value;
                    break;
                }
                else if (auto name = expr->as<Luau::AstExprGlobal>())
                {
                    item.name = name->name.value;
                    break;
                }
                else if (auto index = expr->as<Luau::AstExprIndexName>())
                {
                    item.name = index->index.value;
                    item.detail = Luau::toString(index->expr);
                    break;
                }

                // Fall back to a simple generated name for now
                item.name = Luau::toString(expr);
            }


            item.kind = lsp::SymbolKind::Function;

            if (auto refTextDocument = fileResolver.getOrCreateTextDocumentFromModuleName(*ftv->definition->definitionModuleName))
            {
                item.uri = refTextDocument->uri();
                item.range = {refTextDocument->convertPosition(ftv->definition->definitionLocation.begin),
                    refTextDocument->convertPosition(ftv->definition->definitionLocation.end)};
                item.selectionRange = {refTextDocument->convertPosition(ftv->definition->originalNameLocation.begin),
                    refTextDocument->convertPosition(ftv->definition->originalNameLocation.end)};
            }
            else
                continue;

            std::vector<lsp::Range> convertedRanges{};
            convertedRanges.reserve(exprs.size());
            for (const auto& expr : exprs)
                convertedRanges.emplace_back(
                    lsp::Range{textDocument->convertPosition(expr->location.begin), textDocument->convertPosition(expr->location.end)});

            lsp::CallHierarchyOutgoingCall outgoingCall{item, convertedRanges};
            result.emplace_back(outgoingCall);
        }
    }

    return result;
}