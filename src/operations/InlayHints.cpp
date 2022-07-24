#include "Luau/Ast.h"
#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/LuauExt.hpp"

bool isLiteral(const Luau::AstExpr* expr)
{
    return expr->is<Luau::AstExprConstantBool>() || expr->is<Luau::AstExprConstantString>() || expr->is<Luau::AstExprConstantNumber>() ||
           expr->is<Luau::AstExprConstantNil>();
}

// Adds a text edit onto the hint so that it can be inserted.
void makeInsertable(lsp::InlayHint& hint, Luau::TypeId ty)
{
    auto result = Luau::toStringDetailed(ty);
    if (result.invalid || result.truncated)
        return;
    hint.textEdits.emplace_back(lsp::TextEdit{{hint.position, hint.position}, ": " + result.name});
}

void makeInsertable(lsp::InlayHint& hint, Luau::TypePackId ty)
{
    auto result = types::toStringReturnTypeDetailed(ty);
    if (result.invalid || result.truncated)
        return;
    hint.textEdits.emplace_back(lsp::TextEdit{{hint.position, hint.position}, ": " + result.name});
}
struct InlayHintVisitor : public Luau::AstVisitor
{
    Luau::ModulePtr module;
    const ClientConfiguration& config;
    std::vector<lsp::InlayHint> hints;
    Luau::ToStringOptions stringOptions;

    explicit InlayHintVisitor(Luau::ModulePtr module, const ClientConfiguration& config)
        : module(module)
        , config(config)

    {
        stringOptions.maxTableLength = 30;
        stringOptions.maxTypeLength = config.inlayHints.typeHintMaxLength;
    }

    bool visit(Luau::AstStatLocal* local) override
    {
        if (!config.inlayHints.variableTypes)
            return true;

        auto scope = Luau::findScopeAtPosition(*module, local->location.begin);
        if (!scope)
            return false;

        for (size_t i = 0; i < local->vars.size; i++)
        {
            auto var = local->vars.data[i];
            if (!var->annotation)
            {
                auto ty = scope->lookup(var);
                if (ty)
                {
                    auto followedTy = Luau::follow(*ty);

                    // If the variable is assigned a function, don't bother showing a hint
                    // since we can already infer stuff from the assigned function
                    if (local->values.size > i)
                    {
                        if (Luau::get<Luau::FunctionTypeVar>(followedTy) && local->values.data[i]->is<Luau::AstExprFunction>())
                            continue;
                    }

                    lsp::InlayHint hint;
                    hint.kind = lsp::InlayHintKind::Type;
                    hint.label = ": " + Luau::toString(followedTy, stringOptions);
                    hint.position = convertPosition(var->location.end);
                    makeInsertable(hint, followedTy);
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
            // Parameter types hint
            if (config.inlayHints.parameterTypes)
            {
                size_t idx = 0;
                for (auto argType : ftv->argTypes)
                {
                    if (func->args.size <= idx)
                        break;

                    auto param = func->args.data[idx];
                    if (!param->annotation)
                    {
                        lsp::InlayHint hint;
                        hint.kind = lsp::InlayHintKind::Type;
                        hint.label = ": " + Luau::toString(argType, stringOptions);
                        hint.position = convertPosition(param->location.end);
                        makeInsertable(hint, argType);
                        hints.emplace_back(hint);
                    }
                    idx++;
                }
            }

            // Add return type annotation
            if (config.inlayHints.functionReturnTypes)
            {
                if (!func->returnAnnotation && func->argLocation)
                {
                    lsp::InlayHint hint;
                    hint.kind = lsp::InlayHintKind::Type;
                    hint.label = ": " + types::toStringReturnType(ftv->retTypes, stringOptions);
                    hint.position = convertPosition(func->argLocation->end);
                    makeInsertable(hint, ftv->retTypes);
                    hints.emplace_back(hint);
                }
            }
        }

        return true;
    }

    bool visit(Luau::AstExprCall* call) override
    {
        if (config.inlayHints.parameterNames == InlayHintsParameterNamesConfig::None)
            return true;

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
                if (!namesIt->has_value())
                    continue;

                auto paramName = (*namesIt)->name;
                auto literal = isLiteral(param);
                if (!isLiteral(param))
                {
                    if (config.inlayHints.parameterNames == InlayHintsParameterNamesConfig::Literals)
                        continue;

                    // If the name somewhat matches the arg name, we can skip the inlay hint
                    if (toLower(Luau::toString(param)) == toLower(std::string(paramName)))
                        continue;
                }

                // TODO: only apply in specific situations
                lsp::InlayHint hint;
                hint.kind = lsp::InlayHintKind::Parameter;
                hint.label = paramName + ":";
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

    InlayHintVisitor visitor{module, config};
    visitor.visit(sourceModule->root);
    return visitor.hints;
}

lsp::InlayHintResult LanguageServer::inlayHint(const lsp::InlayHintParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->inlayHint(params);
}