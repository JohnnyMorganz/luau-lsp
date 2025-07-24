#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"

#include "Luau/Ast.h"
#include "Luau/AstQuery.h"
#include "Luau/ToString.h"
#include "Luau/Transpiler.h"
#include "LSP/LuauExt.hpp"

bool isLiteral(const Luau::AstExpr* expr)
{
    return expr->is<Luau::AstExprConstantBool>() || expr->is<Luau::AstExprConstantString>() || expr->is<Luau::AstExprConstantNumber>() ||
           expr->is<Luau::AstExprConstantNil>();
}

// Function with no statements in body
bool isNoOpFunction(const Luau::AstExprFunction* func)
{
    return func->body->body.size == 0;
}

// Adds a text edit onto the hint so that it can be inserted.
void makeInsertable(const ClientConfiguration& config, lsp::InlayHint& hint, Luau::TypeId ty)
{
    if (!config.inlayHints.makeInsertable)
        return;

    Luau::ToStringOptions opts;
    auto result = Luau::toStringDetailed(ty, opts);
    if (result.invalid || result.truncated || result.error || result.cycle)
        return;
    hint.textEdits.emplace_back();
}

void makeInsertable(const ClientConfiguration& config, lsp::InlayHint& hint, Luau::TypePackId ty, bool removeLeadingEllipsis = false)
{
    if (!config.inlayHints.makeInsertable)
        return;

    auto result = types::toStringReturnTypeDetailed(ty);
    if (result.invalid || result.truncated || result.error || result.cycle)
        return;
    auto name = result.name;
    if (removeLeadingEllipsis)
        name = removePrefix(name, "...");
    hint.textEdits.emplace_back(lsp::TextEdit{{hint.position, hint.position}, ": " + name});
}
struct InlayHintVisitor : public Luau::AstVisitor
{
    WorkspaceFileResolver* fileResolver = nullptr;
    const Luau::ModulePtr& module;
    const ClientConfiguration& config;
    const TextDocument* textDocument;
    std::vector<lsp::InlayHint> hints{};
    Luau::ToStringOptions stringOptions;

    explicit InlayHintVisitor(
        WorkspaceFileResolver* fileResolver, const Luau::ModulePtr& module, const ClientConfiguration& config, const TextDocument* textDocument)
        : fileResolver(fileResolver)
        , module(module)
        , config(config)
        , textDocument(textDocument)

    {
        stringOptions.maxTableLength = 30;
        stringOptions.maxTypeLength = config.inlayHints.typeHintMaxLength;
    }

    void createInlayHintForType(const Luau::TypeId& ty, const lsp::Position& position,
        const std::optional<std::function<bool(const std::string&)>>& shouldSkip = std::nullopt)
    {
        auto result = Luau::toStringDetailed(ty, stringOptions);
        if (shouldSkip && (*shouldSkip)(result.name))
            return;

        lsp::InlayHint hint;
        hint.kind = lsp::InlayHintKind::Type;
        hint.position = position;

        hint.label.emplace_back(lsp::InlayHintLabelPart{": "});

        // TODO: Decompose this further with https://github.com/luau-lang/luau/issues/1591
        std::optional<lsp::Location> goToLocation = std::nullopt;
        if (auto location = getLocation(ty))
        {
            if (auto definitionModuleName = Luau::getDefinitionModuleName(ty))
            {
                if (auto document = fileResolver->getOrCreateTextDocumentFromModuleName(*definitionModuleName))
                {
                    goToLocation = lsp::Location{
                        document->uri(), lsp::Range{document->convertPosition(location->begin), document->convertPosition(location->end)}};
                }
            }
        }
        hint.label.emplace_back(lsp::InlayHintLabelPart{result.name, std::nullopt, goToLocation});

        if (config.inlayHints.makeInsertable && !(result.invalid || result.truncated || result.error || result.cycle))
            hint.textEdits.emplace_back(lsp::TextEdit{{hint.position, hint.position}, ": " + result.name});

        hints.emplace_back(hint);
    }

    void createInlayHintForType(const Luau::TypePackId& ty, const lsp::Position& position, bool forVarArg = false)
    {
        auto typeString = types::toStringReturnType(ty, stringOptions);
        if (forVarArg)
            typeString = removePrefix(typeString, "...");

        lsp::InlayHint hint;
        hint.kind = lsp::InlayHintKind::Type;
        hint.label.emplace_back(lsp::InlayHintLabelPart{": " + typeString});
        hint.position = position;
        makeInsertable(config, hint, ty, forVarArg);
        hints.emplace_back(hint);
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
                        if (Luau::get<Luau::FunctionType>(followedTy) && local->values.data[i]->is<Luau::AstExprFunction>())
                            continue;
                    }

                    // If the variable is named "_", don't include an inlay hint
                    if (var->name == "_")
                        continue;

                    if (config.inlayHints.hideHintsForErrorTypes && Luau::get<Luau::ErrorType>(followedTy))
                        continue;

                    createInlayHintForType(followedTy, textDocument->convertPosition(var->location.end),
                        [var](const std::string& typeString)
                        {
                            // If the stringified type is equivalent to the variable name, don't bother
                            // showing an inlay hint
                            return Luau::equalsLower(typeString, var->name.value);
                        });
                }
            }
        }

        return true;
    }

    bool visit(Luau::AstStatForIn* forIn) override
    {
        if (!config.inlayHints.variableTypes)
            return true;

        auto scope = Luau::findScopeAtPosition(*module, forIn->location.begin);
        if (!scope)
            return false;

        for (size_t i = 0; i < forIn->vars.size; i++)
        {
            auto var = forIn->vars.data[i];
            if (!var->annotation)
            {
                auto ty = scope->lookup(var);
                if (ty)
                {
                    auto followedTy = Luau::follow(*ty);

                    // If the variable is named "_", don't include an inlay hint
                    if (var->name == "_")
                        continue;

                    if (config.inlayHints.hideHintsForErrorTypes && Luau::get<Luau::ErrorType>(followedTy))
                        continue;

                    createInlayHintForType(followedTy, textDocument->convertPosition(var->location.end),
                        [var](const std::string& typeString)
                        {
                            // If the stringified type is equivalent to the variable name, don't bother
                            // showing an inlay hint
                            return Luau::equalsLower(typeString, var->name.value);
                        });
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
        if (auto ftv = Luau::get<Luau::FunctionType>(followedTy))
        {
            // Add return type annotation
            if (config.inlayHints.functionReturnTypes)
            {
                if (!func->returnAnnotation && func->argLocation && !isNoOpFunction(func))
                    createInlayHintForType(ftv->retTypes, textDocument->convertPosition(func->argLocation->end));
            }

            // Parameter types hint
            if (config.inlayHints.parameterTypes)
            {
                auto it = Luau::begin(ftv->argTypes);
                if (it != Luau::end(ftv->argTypes))
                {
                    // Skip first item if it is self
                    if (func->self && isMethod(ftv))
                        it++;

                    for (auto param : func->args)
                    {
                        if (it == Luau::end(ftv->argTypes))
                            break;

                        auto argType = *it;
                        if (!param->annotation && param->name != "_")
                            createInlayHintForType(argType, textDocument->convertPosition(param->location.end));

                        it++;
                    }
                }

                if (func->vararg && it.tail())
                {
                    auto varargType = *it.tail();
                    if (!func->varargAnnotation)
                    {
                        createInlayHintForType(varargType, textDocument->convertPosition(func->varargLocation.end), /* forVarArg= */ true);
                    }
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
        if (auto ftv = Luau::get<Luau::FunctionType>(followedTy))
        {
            if (ftv->argNames.size() == 0)
                return true;

            auto namesIt = ftv->argNames.begin();
            auto idx = 0;
            for (auto param : call->args)
            {
                // Skip first item if method call (`:`)
                if (idx == 0 && call->self)
                    namesIt++;

                if (namesIt == ftv->argNames.end())
                    break;

                if (!namesIt->has_value())
                {
                    namesIt++;
                    idx++;
                    continue;
                }

                auto createHint = true;
                auto paramName = (*namesIt)->name;
                if (!isLiteral(param))
                {
                    if (config.inlayHints.parameterNames == InlayHintsParameterNamesConfig::Literals)
                        createHint = false;

                    // If the name somewhat matches the arg name, we can skip the inlay hint
                    std::string stringifiedParam = Luau::toString(param);
                    if (auto indexName = param->as<Luau::AstExprIndexName>())
                        stringifiedParam = Luau::toString(indexName->index);
                    if (config.inlayHints.hideHintsForMatchingParameterNames && Luau::equalsLower(stringifiedParam, paramName))
                        createHint = false;
                }

                // Ignore the parameter name if its just "_"
                if (paramName == "_")
                    createHint = false;

                // TODO: only apply in specific situations
                if (createHint)
                {
                    lsp::InlayHint hint;
                    hint.kind = lsp::InlayHintKind::Parameter;
                    hint.label.emplace_back(lsp::InlayHintLabelPart{paramName + ":"});
                    hint.position = textDocument->convertPosition(param->location.begin);
                    hint.paddingRight = true;
                    hints.emplace_back(hint);
                }

                namesIt++;
                idx++;
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
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());

    // TODO: expressiveTypes - remove "forAutocomplete" once the types have been fixed
    checkStrict(moduleName, /* forAutocomplete: */ config.hover.strictDatamodelTypes);

    auto sourceModule = frontend.getSourceModule(moduleName);
    auto module = getModule(moduleName, /* forAutocomplete: */ config.hover.strictDatamodelTypes);
    if (!sourceModule || !module)
        return {};

    InlayHintVisitor visitor{&fileResolver, module, config, textDocument};
    visitor.visit(sourceModule->root);

    return visitor.hints;
}

lsp::InlayHintResult LanguageServer::inlayHint(const lsp::InlayHintParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->inlayHint(params);
}
