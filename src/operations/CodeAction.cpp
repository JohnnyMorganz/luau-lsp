#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"
#include "Protocol/CodeAction.hpp"
#include "LSP/LuauExt.hpp"
#include "Luau/Transpiler.h"

struct RequireSorter : Luau::AstVisitor
{
    Luau::DenseHashMap<Luau::AstLocal*, Luau::AstExpr*> locals;
    std::vector<Luau::AstExpr*> work;
    std::vector<Luau::AstExprCall*> requireCalls;

    RequireSorter()
        : locals(nullptr)
    {
    }

    bool visit(Luau::AstExprCall* expr) override
    {
        auto* global = expr->func->as<Luau::AstExprGlobal>();

        if (global && global->name == "require" && expr->args.size >= 1)
            requireCalls.push_back(expr);

        return true;
    }

    bool visit(Luau::AstStatLocal* stat) override
    {
        for (size_t i = 0; i < stat->vars.size && i < stat->values.size; ++i)
        {
            Luau::AstLocal* local = stat->vars.data[i];
            Luau::AstExpr* expr = stat->values.data[i];

            // track initializing expression to be able to trace modules through locals
            locals[local] = expr;
        }

        return true;
    }

    bool visit(Luau::AstStatAssign* stat) override
    {
        for (size_t i = 0; i < stat->vars.size; ++i)
        {
            // locals that are assigned don't have a known expression
            if (auto* expr = stat->vars.data[i]->as<Luau::AstExprLocal>())
                locals[expr->local] = nullptr;
        }

        return true;
    }

    // // Called on the expression passed to a require call: require(expr)
    // // We need to see if there exists any variable depedency on the expr (i.e., "Modules" in require(Modules.Roact))
    // // If so, we must ensure that the sorting does not move the require to before this dependency
    // Luau::AstExpr* findVariableDependency(Luau::AstExpr* node)
    // {
    //     if (auto* expr = node->as<Luau::AstExprLocal>())
    //         return locals[expr->local];
    //     else if (auto* expr = node->as<AstExprIndexName>())
    //         return expr->expr;
    //     else if (auto* expr = node->as<AstExprIndexExpr>())
    //         return expr->expr;
    //     else if (auto* expr = node->as<AstExprCall>(); expr && expr->self)
    //         return expr->func->as<AstExprIndexName>()->expr;
    //     else
    //         return nullptr;
    // }

    // void process()
    // {
    //     ModuleInfo moduleContext{currentModuleName};

    //     // seed worklist with require arguments
    //     work.reserve(requireCalls.size());

    //     for (AstExprCall* require : requireCalls)
    //         work.push_back(require->args.data[0]);

    //     // push all dependent expressions to the work stack; note that the vector is modified during traversal
    //     for (size_t i = 0; i < work.size(); ++i)
    //         if (AstExpr* dep = getDependent(work[i]))
    //             work.push_back(dep);

    //     // resolve all expressions to a module info
    //     for (size_t i = work.size(); i > 0; --i)
    //     {
    //         AstExpr* expr = work[i - 1];

    //         // when multiple expressions depend on the same one we push it to work queue multiple times
    //         if (result.exprs.contains(expr))
    //             continue;

    //         std::optional<ModuleInfo> info;

    //         if (AstExpr* dep = getDependent(expr))
    //         {
    //             const ModuleInfo* context = result.exprs.find(dep);

    //             // locals just inherit their dependent context, no resolution required
    //             if (expr->is<AstExprLocal>())
    //                 info = context ? std::optional<ModuleInfo>(*context) : std::nullopt;
    //             else
    //                 info = fileResolver->resolveModule(context, expr);
    //         }
    //         else
    //         {
    //             info = fileResolver->resolveModule(&moduleContext, expr);
    //         }

    //         if (info)
    //             result.exprs[expr] = std::move(*info);
    //     }

    //     // resolve all requires according to their argument
    //     result.requireList.reserve(requireCalls.size());

    //     for (AstExprCall* require : requireCalls)
    //     {
    //         AstExpr* arg = require->args.data[0];

    //         if (const ModuleInfo* info = result.exprs.find(arg))
    //         {
    //             result.requireList.push_back({info->name, require->location});

    //             ModuleInfo infoCopy = *info; // copy *info out since next line invalidates info!
    //             result.exprs[require] = std::move(infoCopy);
    //         }
    //         else
    //         {
    //             result.exprs[require] = {}; // mark require as unresolved
    //         }
    //     }
    // }
};

lsp::WorkspaceEdit WorkspaceFolder::computeOrganiseRequiresEdit(const lsp::DocumentUri& uri)
{
    auto moduleName = fileResolver.getModuleName(uri);
    auto textDocument = fileResolver.getTextDocument(uri);

    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + uri.toString());

    if (frontend.isDirty(moduleName))
        frontend.check(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return {};


    // Get all the requires from the module
    // DocumentSymbolsVisitor visitor{textDocument};
    // visitor.visit(sourceModule->root);
    // return visitor.symbols;
    return {};


    // Sort the requires appropriately
    // Here: we need to special case for Roblox requires, as the ordering for these requires matter
}

lsp::WorkspaceEdit WorkspaceFolder::computeOrganiseServicesEdit(const lsp::DocumentUri& uri)
{
    auto moduleName = fileResolver.getModuleName(uri);
    auto textDocument = fileResolver.getTextDocument(uri);

    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + uri.toString());

    if (frontend.isDirty(moduleName))
        frontend.check(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return {};

    // Find all the `local X = game:GetService("Service")` calls
    FindServicesVisitor visitor;
    visitor.visit(sourceModule->root);

    std::vector<lsp::TextEdit> edits;
    // We firstly delete all the previous services, as they will be added later
    for (const auto& [_, stat] : visitor.serviceLineMap)
        edits.emplace_back(lsp::TextEdit{{{stat->location.begin.line, 0}, {stat->location.begin.line + 1, 0}}, ""});

    // We find the first line to add these services to, and then add them in sorted order
    lsp::Range insertLocation{{visitor.firstServiceDefinitionLine.value(), 0}, {visitor.firstServiceDefinitionLine.value(), 0}};
    for (const auto& [serviceName, stat] : visitor.serviceLineMap)
    {
        // We need to rewrite the statement as we expected it
        auto importText = Luau::toString(stat) + "\n";
        edits.emplace_back(lsp::TextEdit{insertLocation, importText});
    }

    lsp::WorkspaceEdit workspaceEdit;
    workspaceEdit.changes.emplace(uri.toString(), edits);
    return workspaceEdit;
}

lsp::CodeActionResult WorkspaceFolder::codeAction(const lsp::CodeActionParams& params)
{
    std::vector<lsp::CodeAction> result;

    auto config = client->getConfiguration(rootUri);

    // Add organise imports code action
    if (params.context.only.empty() || contains(params.context.only, lsp::CodeActionKind::Source) ||
        contains(params.context.only, lsp::CodeActionKind::SourceOrganizeImports))
    {
        // Add sort requires code action
        lsp::CodeAction organiseImportsAction;
        organiseImportsAction.title = "Sort requires";
        organiseImportsAction.kind = lsp::CodeActionKind::SourceOrganizeImports;

        // // If we can resolve, defer computation till later
        // if (client->capabilities.textDocument && client->capabilities.textDocument->codeAction &&
        //     client->capabilities.textDocument->codeAction->resolveSupport &&
        //     contains(client->capabilities.textDocument->codeAction->resolveSupport->properties, "edit"))
        // {
        //     // Store relevant information in the data field
        // }
        // else
        // {
        //     // Compute the workspace edit now
        // }
        organiseImportsAction.edit = computeOrganiseRequiresEdit(params.textDocument.uri);

        result.emplace_back(organiseImportsAction);


        // If in Roblox mode, add a sort services code action
        if (config.types.roblox)
        {
            lsp::CodeAction sortServicesAction;
            sortServicesAction.title = "Sort services";
            sortServicesAction.kind = lsp::CodeActionKind::SourceOrganizeImports;
            sortServicesAction.edit = computeOrganiseServicesEdit(params.textDocument.uri);
            result.emplace_back(sortServicesAction);
        }
    }

    return result;
}

lsp::CodeActionResult LanguageServer::codeAction(const lsp::CodeActionParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->codeAction(params);
}

lsp::CodeAction LanguageServer::codeActionResolve(const lsp::CodeAction& params)
{
    // TODO
    return params;
}