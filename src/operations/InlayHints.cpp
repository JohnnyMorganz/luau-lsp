#include "Luau/Ast.h"
#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/LuauExt.hpp"


struct InlayHintVisitor : public Luau::AstVisitor
{
    Luau::ModulePtr module;
    std::vector<lsp::InlayHint> hints;

    explicit InlayHintVisitor(Luau::ModulePtr module)
        : module(module)
    {
    }

    bool visit(Luau::AstStatLocal* local) override
    {
        auto scope = Luau::findScopeAtPosition(*module, local->location.begin);
        if (!scope)
            return false;

        for (auto var : local->vars)
        {
            if (!var->annotation)
            {
                auto ty = scope->lookup(var);
                if (ty)
                {
                    auto followedTy = Luau::follow(*ty);

                    lsp::InlayHint hint;
                    hint.kind = lsp::InlayHintKind::Type;
                    hint.label = ": " + Luau::toString(followedTy);
                    hint.position = convertPosition(var->location.end);
                    hints.emplace_back(hint);
                }
            }
        }

        return true;
    }

    bool visit(Luau::AstExprFunction* func) override
    {
        auto ty = module->astTypes.find(func);
        if (!ty)
            return false;

        auto followedTy = Luau::follow(*ty);
        if (auto ftv = Luau::get<Luau::FunctionTypeVar>(followedTy))
        {
            size_t idx = 0;
            for (auto argType : ftv->argTypes)
            {
                auto param = func->args.data[idx];
                if (!param->annotation)
                {
                    lsp::InlayHint hint;
                    hint.kind = lsp::InlayHintKind::Type;
                    hint.label = ": " + Luau::toString(argType);
                    hint.position = convertPosition(param->location.end);
                    hints.emplace_back(hint);
                }
                idx++;
            }

            // Add return type annotation
            if (!func->returnAnnotation && func->argLocation)
            {
                lsp::InlayHint hint;
                hint.kind = lsp::InlayHintKind::Type;
                hint.label = ": " + types::toStringReturnType(ftv->retTypes);
                hint.position = convertPosition(func->argLocation->end);
                hints.emplace_back(hint);
            }
        }

        return true;
    }

    bool visit(Luau::AstExprCall* call) override
    {
        auto ty = module->astTypes.find(call->func);
        if (!ty)
            return false;

        auto followedTy = Luau::follow(*ty);
        if (auto ftv = Luau::get<Luau::FunctionTypeVar>(followedTy))
        {
            auto namesIt = ftv->argNames.begin();
            for (auto param : call->args)
            {
                if (namesIt == ftv->argNames.end())
                    break;

                // TODO: only apply in specific situations
                lsp::InlayHint hint;
                hint.kind = lsp::InlayHintKind::Parameter;
                hint.label = (*namesIt)->name + ":";
                hint.position = convertPosition(param->location.begin);
                hint.paddingRight = true;
                hints.emplace_back(hint);

                namesIt++;
            }
        }

        return true;
    }

    bool visit(Luau::AstStatBlock* block) override
    {
        for (Luau::AstStat* stat : block->body)
        {
            stat->visit(this);
        }

        return false;
    }
};

lsp::InlayHintResult WorkspaceFolder::inlayHint(const lsp::InlayHintParams& params)
{
    auto config = client->getConfiguration(rootUri);

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    std::vector<lsp::DocumentLink> result;

    // TODO: expressiveTypes - remove "forAutocomplete" once the types have been fixed
    frontend.check(moduleName, Luau::FrontendOptions{true, true});

    auto sourceModule = frontend.getSourceModule(moduleName);
    auto module = frontend.moduleResolverForAutocomplete.getModule(moduleName);
    if (!sourceModule || !module)
        return {};

    InlayHintVisitor visitor{module};
    visitor.visit(sourceModule->root);
    return visitor.hints;
}

lsp::InlayHintResult LanguageServer::inlayHint(const lsp::InlayHintParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->inlayHint(params);
}