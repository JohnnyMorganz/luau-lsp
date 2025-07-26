#include <LSP/Workspace.hpp>
#include <LSP/LuauExt.hpp>
#include <Luau/AstQuery.h>
#include <Luau/Transpiler.h>

using FunctionName = std::pair<std::string, std::optional<std::string>>;
static FunctionName getFunctionName(Luau::AstExpr* expr)
{
    if (auto name = expr->as<Luau::AstExprLocal>())
    {
        return {name->local->name.value, std::nullopt};
    }
    else if (auto name = expr->as<Luau::AstExprGlobal>())
    {
        return {name->name.value, std::nullopt};
    }
    else if (auto index = expr->as<Luau::AstExprIndexName>())
    {
        std::string detail = Luau::toString(index->expr);
        trim(detail);
        return {index->index.value, detail};
    }

    // Fall back to a simple generated name
    std::string name = Luau::toString(expr);
    trim(name);
    return {name, std::nullopt};
}

static bool isSameFunction(const Luau::TypeId a, const Luau::TypeId b)
{
    if (a == b)
        return true;

    if (auto ftv1 = Luau::get<Luau::FunctionType>(a))
        if (auto ftv2 = Luau::get<Luau::FunctionType>(b))
            return ftv1->definition && ftv2->definition && ftv1->definition->definitionModuleName && ftv2->definition->definitionModuleName &&
                   ftv1->definition->definitionModuleName == ftv2->definition->definitionModuleName &&
                   ftv1->definition->definitionLocation == ftv2->definition->definitionLocation;

    return false;
}

static Luau::TypeId lookupFunctionCallType(Luau::ModulePtr module, const Luau::AstExprCall* call)
{
    if (auto ty = module->astTypes.find(call->func))
        return Luau::follow(*ty);

    if (auto index = call->func->as<Luau::AstExprIndexName>())
    {
        if (auto parentIt = module->astTypes.find(index->expr))
        {
            auto parentType = Luau::follow(*parentIt);
            if (auto prop = lookupProp(parentType, index->index.value); prop && prop->second.readTy)
                return Luau::follow(*prop->second.readTy);
        }
    }

    return nullptr;
}

struct FindAllFunctionsVisitor : public Luau::AstVisitor
{
    // func name, func location, name location, func body
    std::vector<std::tuple<FunctionName, Luau::Location, Luau::Location, Luau::AstExprFunction*>> funcs;

    explicit FindAllFunctionsVisitor() {}

    bool visit(class Luau::AstStatLocalFunction* node) override
    {
        funcs.emplace_back(FunctionName{node->name->name.value, std::nullopt}, node->location, node->name->location, node->func);
        return true;
    }

    bool visit(class Luau::AstStatFunction* node) override
    {
        funcs.emplace_back(getFunctionName(node->name), node->location, node->name->location, node->func);
        return true;
    };
};

struct FindAllCallsVisitor : public Luau::AstVisitor
{
    std::vector<const Luau::AstExprCall*> calls;
    bool ignoreOtherFunctions;

    explicit FindAllCallsVisitor(bool ignoreOtherFunctions = false)
        : ignoreOtherFunctions(ignoreOtherFunctions)
    {
    }

    bool visit(class Luau::AstExprCall* node) override
    {
        calls.emplace_back(node);
        return true;
    }

    bool visit(class Luau::AstStatLocalFunction*) override
    {
        return !ignoreOtherFunctions;
    }

    bool visit(class Luau::AstStatFunction* node) override
    {
        return !ignoreOtherFunctions;
    };
};


std::vector<lsp::CallHierarchyItem> WorkspaceFolder::prepareCallHierarchy(
    const lsp::CallHierarchyPrepareParams& params, const LSPCancellationToken& cancellationToken)
{
    // TODO: this is largely based off goto definition, maybe DRY?

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());
    auto position = textDocument->convertPosition(params.position);

    // Run the type checker to ensure we are up to date
    checkStrict(moduleName, cancellationToken);
    throwIfCancelled(cancellationToken);

    auto sourceModule = frontend.getSourceModule(moduleName);
    auto module = getModule(moduleName, /* forAutocomplete: */ true);
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
    else if (auto type = module->astTypes.find(exprOrLocal.getExpr()))
    {
        ty = Luau::follow(*type);
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
            auto functionName = getFunctionName(exprOrLocal.getExpr());
            item.name = functionName.first;
            item.detail = functionName.second;
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
    auto moduleName = fileResolver.getModuleName(params.item.uri);

    // NOTE: the text document in this case may not necessarily be managed
    auto textDocument = fileResolver.getOrCreateTextDocumentFromModuleName(moduleName);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No text document available for " + params.item.uri.toString());
    auto position = textDocument->convertPosition(params.item.selectionRange.start);

    // Find the definition of the original function, to determine the appropriate TypeId to lookup
    auto sourceModule = frontend.getSourceModule(moduleName);
    auto module = getModule(moduleName, /* forAutocomplete: */ true);
    if (!sourceModule || !module)
        return {};
    auto node = Luau::findExprAtPosition(*sourceModule, position);
    if (!node || !node->is<Luau::AstExprFunction>())
        return {};

    auto func = node->as<Luau::AstExprFunction>();
    auto ty = module->astTypes.find(func);
    if (!ty)
        return {};

    auto followedTy = Luau::follow(*ty);

    std::vector<lsp::CallHierarchyIncomingCall> result;

    // Find all reverse dependencies of this module
    std::vector<Luau::ModuleName> dependents = findReverseDependencies(moduleName);

    // For each module, search for callers
    for (const auto& dependentModuleName : dependents)
    {
        auto dependentSourceModule = frontend.getSourceModule(dependentModuleName);
        auto dependentModule = getModule(dependentModuleName, /* forAutocomplete: */ true);
        if (!dependentSourceModule || !dependentModule)
            continue;

        FindAllFunctionsVisitor funcsVisitor;
        dependentSourceModule->root->visit(&funcsVisitor);

        auto findIncomingCalls = [this, followedTy, &dependentModuleName, &dependentModule, &result](Luau::AstNode* node,
                                     std::optional<FunctionName> funcName, std::optional<std::pair<Luau::Location, Luau::Location>> locations)
        {
            FindAllCallsVisitor callsVisitor(/* ignoreOtherFunctions = */ true);
            node->visit(&callsVisitor);
            if (callsVisitor.calls.empty())
                return;

            // Check if any of the calls match
            std::vector<const Luau::AstExprCall*> matchingCalls{};
            for (const auto& call : callsVisitor.calls)
                if (auto ty2 = lookupFunctionCallType(dependentModule, call))
                    if (isSameFunction(ty2, followedTy))
                        matchingCalls.emplace_back(call);

            if (matchingCalls.empty())
                return;

            lsp::CallHierarchyItem item{};

            if (funcName)
            {
                item.name = funcName->first;
                item.detail = funcName->second;
                item.kind = lsp::SymbolKind::Function;
            }
            else
            {
                item.name = "<no function>";
                item.kind = lsp::SymbolKind::Namespace;
            }

            std::vector<lsp::Range> convertedRanges{};
            convertedRanges.reserve(matchingCalls.size());

            if (auto refTextDocument = fileResolver.getOrCreateTextDocumentFromModuleName(dependentModuleName))
            {
                item.uri = refTextDocument->uri();

                if (locations)
                {
                    auto [funcLocation, nameLocation] = locations.value();
                    item.range = {refTextDocument->convertPosition(funcLocation.begin), refTextDocument->convertPosition(funcLocation.end)};
                    item.selectionRange = {refTextDocument->convertPosition(nameLocation.begin), refTextDocument->convertPosition(nameLocation.end)};
                }
                else
                {
                    item.range = {{0, 0}, {refTextDocument->lineCount() - 1, 0}};
                    item.selectionRange = {{0, 0}, {0, 0}};
                }

                for (const auto& call : matchingCalls)
                    convertedRanges.emplace_back(lsp::Range{
                        refTextDocument->convertPosition(call->func->location.begin), refTextDocument->convertPosition(call->func->location.end)});
            }
            else
                return;

            lsp::CallHierarchyIncomingCall incomingCall{item, convertedRanges};
            result.emplace_back(incomingCall);
        };

        for (auto& [funcName, funcLocation, nameLocation, func] : funcsVisitor.funcs)
        {
            findIncomingCalls(func, funcName, std::make_pair(funcLocation, nameLocation));
        }

        // Search the root of the AST to find calls outside of functions
        findIncomingCalls(dependentSourceModule->root, std::nullopt, std::nullopt);
    }

    return result;
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
    auto module = getModule(moduleName, /* forAutocomplete: */ true);
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
        if (auto ty = lookupFunctionCallType(module, call))
        {
            if (!contains(calls, ty))
                calls.emplace(ty, std::vector<Luau::AstExpr*>{});
            calls.at(ty).emplace_back(call->func);
        }
    }

    // Convert calls into hierarchy items
    for (const auto& [ty, exprs] : calls)
    {
        if (auto ftv = Luau::get<Luau::FunctionType>(ty); ftv && ftv->definition && ftv->definition->definitionModuleName)
        {
            lsp::CallHierarchyItem item{};

            // Use the first call to determine name
            auto funcName = getFunctionName(exprs[0]);
            item.name = funcName.first;
            item.detail = funcName.second;

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
