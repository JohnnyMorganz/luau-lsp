#include "LSP/DocumentationParser.hpp"
#include "LSP/Workspace.hpp"

#include <algorithm>

#include "Luau/Ast.h"
#include "Luau/AstQuery.h"
#include "Luau/ToString.h"
#include "Luau/PrettyPrinter.h"
#include "LSP/LuauExt.hpp"

static std::vector<lsp::InlayHintLabelPart> toInlayHintLabelParts(
    const Luau::ToStringResult& result, const Client* client, WorkspaceFileResolver* fileResolver)
{
    std::vector<lsp::InlayHintLabelPart> parts;

    if (result.typeSpans.empty())
    {
        parts.push_back(lsp::InlayHintLabelPart{result.name});
        return parts;
    }

    auto spans = result.typeSpans;
    std::sort(spans.begin(), spans.end(),
        [](const Luau::ToStringSpan& a, const Luau::ToStringSpan& b)
        {
            return a.startPos < b.startPos;
        });

    size_t nameLen = result.name.length();
    size_t lastEnd = 0;
    for (const auto& [start, end, typeId] : spans)
    {
        if (start >= nameLen)
            break;

        size_t clampedEnd = std::min(end, nameLen);

        if (start > lastEnd)
            parts.emplace_back(lsp::InlayHintLabelPart{result.name.substr(lastEnd, start - lastEnd)});

        lsp::InlayHintLabelPart part;
        part.value = result.name.substr(start, clampedEnd - start);
        part.location = types::getTypeLocation(typeId, fileResolver);
        if (typeId->documentationSymbol)
        {
            if (auto documentation = printDocumentation(client->documentation, *typeId->documentationSymbol))
                part.tooltip = lsp::MarkupContent{lsp::MarkupKind::Markdown, *documentation};
        }
        parts.push_back(part);

        lastEnd = clampedEnd;
    }

    if (lastEnd < nameLen)
        parts.emplace_back(lsp::InlayHintLabelPart{result.name.substr(lastEnd)});

    return parts;
}

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
    hint.textEdits.emplace_back(lsp::TextEdit{{hint.position, hint.position}, ": " + result.name});
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
    const Luau::ModulePtr& module;
    const ClientConfiguration& config;
    const Client* client;
    const TextDocument* textDocument;
    WorkspaceFileResolver* fileResolver;
    WorkspaceFolder* workspace;
    std::vector<lsp::InlayHint> hints{};
    Luau::ToStringOptions stringOptions;

    explicit InlayHintVisitor(const Luau::ModulePtr& module, const ClientConfiguration& config, const Client* client,
        const TextDocument* textDocument, WorkspaceFileResolver* fileResolver, WorkspaceFolder* workspace = nullptr)
        : module(module)
        , config(config)
        , client(client)
        , textDocument(textDocument)
        , fileResolver(fileResolver)
        , workspace(workspace)

    {
        stringOptions.maxTableLength = 30;
        stringOptions.maxTypeLength = config.inlayHints.typeHintMaxLength;
    }

    void setLabelFromType(lsp::InlayHint& hint, Luau::TypeId ty, const std::string& prefix = ": ")
    {
        auto result = Luau::toStringDetailed(ty, stringOptions);
        hint.label.push_back(lsp::InlayHintLabelPart{prefix});
        auto parts = toInlayHintLabelParts(result, client, fileResolver);
        hint.label.insert(hint.label.end(), parts.begin(), parts.end());
    }

    void setLabelFromTypePack(lsp::InlayHint& hint, Luau::TypePackId ty, const std::string& prefix = ": ", bool removeEllipsis = false)
    {
        auto result = types::toStringReturnTypeDetailed(ty, stringOptions);
        if (removeEllipsis)
            result.name = removePrefix(result.name, "...");
        hint.label.push_back(lsp::InlayHintLabelPart{prefix});
        auto parts = toInlayHintLabelParts(result, client, fileResolver);
        hint.label.insert(hint.label.end(), parts.begin(), parts.end());
    }

    // A module that exports only types returns an empty table, so the hint for a local bound to it read `: {}`.
    // That says nothing about the module, and inserting it adds an annotation with no meaning.
    // Instead the hint names the types the module exports, and it cannot be inserted.
    // Each name shows the documentation of its type and links to its definition.
    std::optional<lsp::InlayHint> typeOnlyModuleHint(const Luau::AstLocal* var, Luau::TypeId ty, const Luau::Scope& scope)
    {
        if (!workspace)
            return std::nullopt;

        auto table = Luau::get<Luau::TableType>(ty);
        if (!table || !table->props.empty() || table->indexer)
            return std::nullopt;

        auto importedName = lookupImportedModule(scope, var->name.value);
        if (!importedName)
            return std::nullopt;

        auto importedModule = workspace->getModule(*importedName, /* forAutocomplete: */ config.hover.strictDatamodelTypes);
        if (!importedModule || importedModule->exportedTypeBindings.empty())
            return std::nullopt;

        // The order the module declares its types in
        std::vector<std::pair<Luau::Location, std::string>> names;
        for (const auto& [name, typeFun] : importedModule->exportedTypeBindings)
            names.emplace_back(typeFun.definitionLocation.value_or(Luau::Location{}), name);
        std::sort(names.begin(), names.end(),
            [](const auto& a, const auto& b)
            {
                return a.first.begin < b.first.begin;
            });

        auto document = fileResolver->getOrCreateTextDocumentFromModuleName(*importedName);

        // `{ type User, type Id }`, cut at the length every type hint keeps
        lsp::InlayHint hint;
        hint.kind = lsp::InlayHintKind::Type;
        hint.position = textDocument->convertPosition(var->location.end);
        hint.label.push_back(lsp::InlayHintLabelPart{": { "});
        size_t length = 4;
        std::string hidden;
        for (size_t i = 0; i < names.size(); i++)
        {
            const auto& [location, name] = names[i];
            std::string entry = "type " + name;
            if (!hidden.empty() || (i > 0 && length + entry.size() + 4 > config.inlayHints.typeHintMaxLength))
            {
                hidden += "\n- `type " + name + "`";
                continue;
            }

            if (i > 0)
                hint.label.push_back(lsp::InlayHintLabelPart{", "});
            length += entry.size() + (i > 0 ? 2 : 0);

            lsp::InlayHintLabelPart part{entry};
            if (document)
                part.location =
                    lsp::Location{document->uri(), lsp::Range{document->convertPosition(location.begin), document->convertPosition(location.end)}};
            if (auto documentation = printMoonwaveDocumentation(workspace->getComments(*importedName, location)); !documentation.empty())
                part.tooltip = lsp::MarkupContent{lsp::MarkupKind::Markdown, documentation};
            hint.label.push_back(part);
        }

        if (!hidden.empty())
        {
            lsp::InlayHintLabelPart more{", ..."};
            more.tooltip = lsp::MarkupContent{lsp::MarkupKind::Markdown, "Also exports:\n" + hidden};
            hint.label.push_back(more);
        }
        hint.label.push_back(lsp::InlayHintLabelPart{" }"});
        return hint;
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

                    auto typeString = Luau::toString(followedTy, stringOptions);

                    // If the stringified type is equivalent to the variable name, don't bother
                    // showing an inlay hint
                    if (Luau::equalsLower(typeString, var->name.value))
                        continue;

                    if (auto typeOnly = typeOnlyModuleHint(var, followedTy, *scope))
                    {
                        hints.emplace_back(*typeOnly);
                        continue;
                    }

                    lsp::InlayHint hint;
                    hint.kind = lsp::InlayHintKind::Type;
                    hint.position = textDocument->convertPosition(var->location.end);
                    setLabelFromType(hint, followedTy);
                    makeInsertable(config, hint, followedTy);
                    hints.emplace_back(hint);
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

                    auto typeString = Luau::toString(followedTy, stringOptions);

                    // If the stringified type is equivalent to the variable name, don't bother
                    // showing an inlay hint
                    if (Luau::equalsLower(typeString, var->name.value))
                        continue;

                    lsp::InlayHint hint;
                    hint.kind = lsp::InlayHintKind::Type;
                    hint.position = textDocument->convertPosition(var->location.end);
                    setLabelFromType(hint, followedTy);
                    makeInsertable(config, hint, followedTy);
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
        if (auto ftv = Luau::get<Luau::FunctionType>(followedTy))
        {
            // Add return type annotation
            if (config.inlayHints.functionReturnTypes)
            {
                if (!func->returnAnnotation && func->argLocation && !isNoOpFunction(func))
                {
                    lsp::InlayHint hint;
                    hint.kind = lsp::InlayHintKind::Type;
                    hint.position = textDocument->convertPosition(func->argLocation->end);
                    setLabelFromTypePack(hint, ftv->retTypes);
                    makeInsertable(config, hint, ftv->retTypes);
                    hints.emplace_back(hint);
                }
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
                        {
                            lsp::InlayHint hint;
                            hint.kind = lsp::InlayHintKind::Type;
                            hint.position = textDocument->convertPosition(param->location.end);
                            setLabelFromType(hint, argType);
                            makeInsertable(config, hint, argType);
                            hints.emplace_back(hint);
                        }

                        it++;
                    }
                }

                if (func->vararg && it.tail())
                {
                    auto varargType = *it.tail();
                    if (!func->varargAnnotation)
                    {
                        lsp::InlayHint hint;
                        hint.kind = lsp::InlayHintKind::Type;
                        hint.position = textDocument->convertPosition(func->varargLocation.end);
                        setLabelFromTypePack(hint, varargType, ": ", /* removeEllipsis: */ true);
                        makeInsertable(config, hint, varargType, /* removeLeadingEllipsis: */ true);
                        hints.emplace_back(hint);
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
                    hint.label.push_back(lsp::InlayHintLabelPart{paramName + ":"});
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

lsp::InlayHintResult WorkspaceFolder::inlayHint(const lsp::InlayHintParams& params, const LSPCancellationToken& cancellationToken)
{
    auto config = client->getConfiguration(rootUri);

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());

    // TODO: expressiveTypes - remove "forAutocomplete" once the types have been fixed
    checkStrict(moduleName, cancellationToken, /* forAutocomplete: */ config.hover.strictDatamodelTypes);
    throwIfCancelled(cancellationToken);

    auto sourceModule = frontend.getSourceModule(moduleName);
    auto module = getModule(moduleName, /* forAutocomplete: */ config.hover.strictDatamodelTypes);
    if (!sourceModule || !module)
        return {};

    InlayHintVisitor visitor{module, config, client, textDocument, &fileResolver, this};
    visitor.visit(sourceModule->root);

    return visitor.hints;
}
