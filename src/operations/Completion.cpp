#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/Protocol.hpp"
#include "LSP/LuauExt.hpp"

/// Defining sort text levels assigned to completion items
/// Note that sort text is lexicographically
namespace SortText
{
static constexpr const char* TableProperties = "0";
static constexpr const char* CorrectTypeKind = "1";
static constexpr const char* CorrectFunctionResult = "2";
static constexpr const char* Default = "3";
static constexpr const char* WrongIndexType = "4";
static constexpr const char* MetatableIndex = "5";
static constexpr const char* AutoImports = "6";
static constexpr const char* Keywords = "7";
} // namespace SortText

void WorkspaceFolder::endAutocompletion(const lsp::CompletionParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto document = fileResolver.getTextDocument(moduleName);
    if (!document)
        return;
    auto position = document->convertPosition(params.position);

    if (frontend.isDirty(moduleName))
        frontend.check(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return;

    auto ancestry = Luau::findAstAncestryOfPosition(*sourceModule, position);
    if (ancestry.size() < 2)
        return;

    Luau::AstNode* parent = ancestry.at(ancestry.size() - 2);
    if (!parent)
        return;

    // We should only apply it if the line just above us is the start of the unclosed statement
    // Otherwise, we insert ends in weird places if theirs an unclosed stat a while away
    if (!parent->is<Luau::AstStatForIn>() && !parent->is<Luau::AstStatFor>() && !parent->is<Luau::AstStatIf>() && !parent->is<Luau::AstStatWhile>() &&
        !parent->is<Luau::AstExprFunction>())
        return;
    if (params.position.line - parent->location.begin.line > 1)
        return;

    auto unclosedBlock = false;
    for (auto it = ancestry.rbegin(); it != ancestry.rend(); ++it)
    {
        if (Luau::AstStatForIn* statForIn = (*it)->as<Luau::AstStatForIn>(); statForIn && !statForIn->hasEnd)
            unclosedBlock = true;
        if (Luau::AstStatFor* statFor = (*it)->as<Luau::AstStatFor>(); statFor && !statFor->hasEnd)
            unclosedBlock = true;
        if (Luau::AstStatIf* statIf = (*it)->as<Luau::AstStatIf>(); statIf && !statIf->hasEnd)
            unclosedBlock = true;
        if (Luau::AstStatWhile* statWhile = (*it)->as<Luau::AstStatWhile>(); statWhile && !statWhile->hasEnd)
            unclosedBlock = true;
        if (Luau::AstExprFunction* exprFunction = (*it)->as<Luau::AstExprFunction>(); exprFunction && !exprFunction->hasEnd)
            unclosedBlock = true;
    }

    // TODO: we could potentially extend this further that just `hasEnd`
    // by inserting `then`, `until` `do` etc. It seems Studio does this

    if (unclosedBlock)
    {
        // If the position marker is at the very end of the file, if we insert one line further then vscode will
        // not be happy and will insert at the position marker.
        // If its in the middle of the file, vscode won't change the marker
        if (params.position.line == document->lineCount() - 1)
        {
            // Insert an end at the current position, with a newline before it
            // We replace all the current contents of the line since it will just be whitespace
            lsp::TextEdit edit{{{params.position.line, 0}, {params.position.line, params.position.character}}, "\nend\n"};
            std::unordered_map<std::string, std::vector<lsp::TextEdit>> changes{{params.textDocument.uri.toString(), {edit}}};
            client->applyEdit({"insert end", {changes}},
                [this](auto) -> void
                {
                    // Move the cursor up
                    // $/command notification has been manually added by us in the extension
                    client->sendNotification("$/command", std::make_optional<json>({
                                                              {"command", "cursorMove"},
                                                              {"data", {{"to", "prevBlankLine"}}},
                                                          }));
                });
        }
        else
        {
            // Find the indentation level to stick the end on
            std::string indent = "";
            if (document->lineCount() > 1)
            {
                // Use the indentation of the previous line, as thats where the stat begins
                auto prevLine = document->getLine(params.position.line - 1);
                if (prevLine.size() > 0)
                {
                    auto ch = prevLine.at(0);
                    if (ch == ' ' || ch == '\t')
                    {
                        for (auto it = prevLine.begin(); it != prevLine.end(); ++it)
                        {
                            if (*it != ch)
                                break;
                            indent += *it;
                        }
                    }
                }
            }

            // Insert the end onto the next line
            lsp::Position position{params.position.line + 1, 0};
            lsp::TextEdit edit{{position, position}, indent + "end\n"};
            std::unordered_map<std::string, std::vector<lsp::TextEdit>> changes{{params.textDocument.uri.toString(), {edit}}};
            client->applyEdit({"insert end", {changes}});
        }
    }
}

bool isGetService(const Luau::AstExpr* expr)
{
    if (auto call = expr->as<Luau::AstExprCall>())
        if (auto index = call->func->as<Luau::AstExprIndexName>())
            if (index->index == "GetService")
                if (auto name = index->expr->as<Luau::AstExprGlobal>())
                    if (name->name == "game")
                        return true;

    return false;
}

struct ImportLocationVisitor : public Luau::AstVisitor
{
    std::optional<size_t> firstServiceDefinitionLine = std::nullopt;
    std::unordered_map<std::string, size_t> serviceLineMap;

    bool visit(Luau::AstStatLocal* local) override
    {
        if (local->vars.size != 1 || local->values.size != 1)
            return false;

        auto localName = local->vars.data[0];
        auto expr = local->values.data[0];

        if (!localName || !expr)
            return false;

        auto line = localName->location.begin.line;

        if (isGetService(expr))
        {
            firstServiceDefinitionLine =
                !firstServiceDefinitionLine.has_value() || firstServiceDefinitionLine.value() >= line ? line : firstServiceDefinitionLine.value();
            serviceLineMap.emplace(std::string(localName->name.value), line);
        }

        return false;
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

/// Attempts to retrieve a list of service names by inspecting the global type definitions
static std::vector<std::string> getServiceNames(const Luau::ScopePtr scope)
{
    std::vector<std::string> services;

    if (auto dataModelType = scope->lookupType("ServiceProvider"))
    {
        if (auto ctv = Luau::get<Luau::ClassTypeVar>(dataModelType->type))
        {
            if (auto getService = Luau::lookupClassProp(ctv, "GetService"))
            {
                if (auto itv = Luau::get<Luau::IntersectionTypeVar>(getService->type))
                {
                    for (auto part : itv->parts)
                    {
                        if (auto ftv = Luau::get<Luau::FunctionTypeVar>(part))
                        {
                            auto it = Luau::begin(ftv->argTypes);
                            auto end = Luau::end(ftv->argTypes);

                            if (it != end && ++it != end)
                            {
                                if (auto stv = Luau::get<Luau::SingletonTypeVar>(*it))
                                {
                                    if (auto ss = Luau::get<Luau::StringSingleton>(stv))
                                    {
                                        services.emplace_back(ss->value);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return services;
}

void WorkspaceFolder::suggestImports(
    const Luau::ModuleName& moduleName, const Luau::Position& position, const ClientConfiguration& config, std::vector<lsp::CompletionItem>& result)
{
    auto sourceModule = frontend.getSourceModule(moduleName);
    auto module = frontend.moduleResolverForAutocomplete.getModule(moduleName);
    if (!sourceModule || !module)
        return;

    // If in roblox mode - suggest services
    if (config.types.roblox)
    {
        auto scope = Luau::findScopeAtPosition(*module, position);
        if (!scope)
            return;

        // Place after any hot comments and TODO: already imported services
        size_t minimumLineNumber = 0;
        for (const auto& hotComment : sourceModule->hotcomments)
        {
            if (!hotComment.header)
                continue;
            if (hotComment.location.begin.line >= minimumLineNumber)
                minimumLineNumber = hotComment.location.begin.line + 1U;
        }

        ImportLocationVisitor visitor;
        visitor.visit(sourceModule->root);

        if (visitor.firstServiceDefinitionLine)
            minimumLineNumber = *visitor.firstServiceDefinitionLine > minimumLineNumber ? *visitor.firstServiceDefinitionLine : minimumLineNumber;

        auto services = getServiceNames(frontend.typeCheckerForAutocomplete.globalScope);
        for (auto& service : services)
        {
            // ASSUMPTION: if the service was defined, it was defined with the exact same name
            bool isAlreadyDefined = false;
            size_t lineNumber = minimumLineNumber;
            for (auto& [definedService, location] : visitor.serviceLineMap)
            {
                if (definedService == service)
                {
                    isAlreadyDefined = true;
                    break;
                }

                if (definedService < service && location >= lineNumber)
                    lineNumber = location + 1;
            }

            if (isAlreadyDefined)
                continue;

            auto importText = "local " + service + " = game:GetService(\"" + service + "\")\n";

            lsp::CompletionItem item;
            item.label = service;
            item.kind = lsp::CompletionItemKind::Class;
            item.detail = "Auto-import";
            item.documentation = {lsp::MarkupKind::Markdown, codeBlock("lua", importText)};
            item.insertText = service;
            item.sortText = SortText::AutoImports;

            lsp::Position placement{lineNumber, 0};
            item.additionalTextEdits.emplace_back(lsp::TextEdit{{placement, placement}, importText});

            result.emplace_back(item);
        }
    }
}

std::vector<lsp::CompletionItem> WorkspaceFolder::completion(const lsp::CompletionParams& params)
{
    auto config = client->getConfiguration(rootUri);

    if (!config.completion.enabled)
        return {};

    if (params.context && params.context->triggerCharacter == "\n")
    {
        if (config.autocompleteEnd)
            endAutocompletion(params);
        return {};
    }

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(moduleName);
    if (!textDocument)
    {
        // TODO: REMOVE TRACE LOGGING
        std::vector<std::string> managed;
        managed.reserve(fileResolver.managedFiles.size());
        for (const auto& [file, _] : fileResolver.managedFiles)
            managed.push_back(file);
        client->sendLogMessage(lsp::MessageType::Error, "managed document info: " + json(managed).dump());
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + moduleName);
    }

    auto position = textDocument->convertPosition(params.position);
    auto result = Luau::autocomplete(frontend, moduleName, position,
        [&](std::string tag, std::optional<const Luau::ClassTypeVar*> ctx) -> std::optional<Luau::AutocompleteEntryMap>
        {
            if (tag == "ClassNames")
            {
                if (auto instanceType = frontend.typeChecker.globalScope->lookupType("Instance"))
                {
                    if (auto* ctv = Luau::get<Luau::ClassTypeVar>(instanceType->type))
                    {
                        Luau::AutocompleteEntryMap result;
                        for (auto& [_, ty] : frontend.typeChecker.globalScope->exportedTypeBindings)
                        {
                            if (auto* c = Luau::get<Luau::ClassTypeVar>(ty.type))
                            {
                                // Check if the ctv is a subclass of instance
                                if (Luau::isSubclass(c, ctv))

                                    result.insert_or_assign(
                                        c->name, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String, frontend.singletonTypes->stringType,
                                                     false, false, Luau::TypeCorrectKind::Correct});
                            }
                        }

                        return result;
                    }
                }
            }
            else if (tag == "Properties")
            {
                if (ctx && ctx.value())
                {
                    Luau::AutocompleteEntryMap result;
                    auto ctv = ctx.value();
                    while (ctv)
                    {
                        for (auto& [propName, _] : ctv->props)
                        {
                            result.insert_or_assign(propName, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String,
                                                                  frontend.singletonTypes->stringType, false, false, Luau::TypeCorrectKind::Correct});
                        }
                        if (ctv->parent)
                            ctv = Luau::get<Luau::ClassTypeVar>(*ctv->parent);
                        else
                            break;
                    }
                    return result;
                }
            }
            else if (tag == "Enums")
            {
                auto it = frontend.typeChecker.globalScope->importedTypeBindings.find("Enum");
                if (it == frontend.typeChecker.globalScope->importedTypeBindings.end())
                    return std::nullopt;

                Luau::AutocompleteEntryMap result;
                for (auto& [enumName, _] : it->second)
                    result.insert_or_assign(enumName, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String,
                                                          frontend.singletonTypes->stringType, false, false, Luau::TypeCorrectKind::Correct});

                return result;
            }

            return std::nullopt;
        });


    std::vector<lsp::CompletionItem> items;

    for (auto& [name, entry] : result.entryMap)
    {
        lsp::CompletionItem item;
        item.label = name;
        item.deprecated = entry.deprecated;
        item.sortText = SortText::Default;

        if (entry.documentationSymbol)
            item.documentation = {lsp::MarkupKind::Markdown, printDocumentation(client->documentation, *entry.documentationSymbol)};

        if (entry.typeCorrect == Luau::TypeCorrectKind::Correct)
            item.sortText = SortText::CorrectTypeKind;
        else if (entry.typeCorrect == Luau::TypeCorrectKind::CorrectFunctionResult)
            item.sortText = SortText::CorrectFunctionResult;
        else if (entry.kind == Luau::AutocompleteEntryKind::Property && types::isMetamethod(name))
            item.sortText = SortText::MetatableIndex;
        else if (entry.wrongIndexType)
            item.sortText = SortText::WrongIndexType;
        else if (entry.kind == Luau::AutocompleteEntryKind::Property)
            item.sortText = SortText::TableProperties;
        else if (entry.kind == Luau::AutocompleteEntryKind::Keyword)
            item.sortText = SortText::Keywords;

        switch (entry.kind)
        {
        case Luau::AutocompleteEntryKind::Property:
            item.kind = lsp::CompletionItemKind::Field;
            break;
        case Luau::AutocompleteEntryKind::Binding:
            item.kind = lsp::CompletionItemKind::Variable;
            break;
        case Luau::AutocompleteEntryKind::Keyword:
            item.kind = lsp::CompletionItemKind::Keyword;
            break;
        case Luau::AutocompleteEntryKind::String:
            item.kind = lsp::CompletionItemKind::Constant; // TODO: is a string autocomplete always a singleton constant?
            break;
        case Luau::AutocompleteEntryKind::Type:
            item.kind = lsp::CompletionItemKind::Interface;
            break;
        case Luau::AutocompleteEntryKind::Module:
            item.kind = lsp::CompletionItemKind::Module;
            break;
        }

        // Handle if name is not an identifier
        if (entry.kind == Luau::AutocompleteEntryKind::Property && !Luau::isIdentifier(name))
        {
            auto lastAst = result.ancestry.back();
            if (auto indexName = lastAst->as<Luau::AstExprIndexName>())
            {
                lsp::TextEdit textEdit;
                textEdit.newText = "[\"" + name + "\"]";
                textEdit.range = {
                    textDocument->convertPosition(indexName->indexLocation.begin), textDocument->convertPosition(indexName->indexLocation.end)};
                item.textEdit = textEdit;

                // For some reason, the above text edit can't handle replacing the index operator
                // Hence we remove it using an additional text edit
                item.additionalTextEdits.emplace_back(lsp::TextEdit{
                    {textDocument->convertPosition(indexName->opPosition), {indexName->opPosition.line, indexName->opPosition.column + 1U}}, ""});
            }
        }

        // Handle parentheses suggestions
        if (config.completion.addParentheses)
        {
            if (entry.parens == Luau::ParenthesesRecommendation::CursorAfter)
            {
                if (item.textEdit)
                    item.textEdit->newText += "()$0";
                else
                    item.insertText = name + "()$0";
                item.insertTextFormat = lsp::InsertTextFormat::Snippet;
            }
            else if (entry.parens == Luau::ParenthesesRecommendation::CursorInside)
            {
                std::string parenthesesSnippet = config.completion.addTabstopAfterParentheses ? "($1)$0" : "($0)";

                if (item.textEdit)
                    item.textEdit->newText += parenthesesSnippet;
                else
                    item.insertText = name + parenthesesSnippet;
                item.insertTextFormat = lsp::InsertTextFormat::Snippet;
                // Trigger Signature Help
                item.command = lsp::Command{"Trigger Signature Help", "editor.action.triggerParameterHints"};
            }
        }

        if (entry.type.has_value())
        {
            auto id = Luau::follow(entry.type.value());
            if (Luau::isOverloadedFunction(id))
                item.kind = lsp::CompletionItemKind::Function;

            // Try to infer more type info about the entry to provide better suggestion info
            if (auto ftv = Luau::get<Luau::FunctionTypeVar>(id))
            {
                item.kind = lsp::CompletionItemKind::Function;

                // Add label details
                // We also create a better detailed parentheses snippet if we are filling arguments
                std::string detail = "(";
                std::string parenthesesSnippet = "(";

                bool comma = false;
                size_t argIndex = 0;
                size_t snippetIndex = 1;

                auto it = Luau::begin(ftv->argTypes);
                for (; it != Luau::end(ftv->argTypes); ++it, ++argIndex)
                {
                    std::string argName = "_";
                    if (argIndex < ftv->argNames.size() && ftv->argNames.at(argIndex))
                        argName = ftv->argNames.at(argIndex)->name;

                    // TODO: hasSelf is not always specified, so we manually check for the "self" name (https://github.com/Roblox/luau/issues/551)
                    if (argIndex == 0 && (ftv->hasSelf || argName == "self"))
                        continue;

                    if (comma)
                    {
                        detail += ", ";
                        parenthesesSnippet += ", ";
                    }

                    detail += argName;
                    parenthesesSnippet += "${" + std::to_string(snippetIndex) + ":" + argName + "}";

                    comma = true;
                    snippetIndex++;
                }

                if (auto tail = it.tail())
                {
                    if (comma)
                    {
                        detail += ", ";
                        parenthesesSnippet += ", ";
                    }
                    detail += Luau::toString(*tail);
                    parenthesesSnippet += "${" + std::to_string(snippetIndex) + ":...}";
                }

                detail += ")";
                parenthesesSnippet += ")";
                item.labelDetails = {detail};

                // If we had CursorAfter, then the function call would not have any arguments
                if (config.completion.fillCallArguments)
                {
                    if (config.completion.addTabstopAfterParentheses)
                        parenthesesSnippet += "$0";

                    if (item.textEdit)
                        item.textEdit->newText += parenthesesSnippet;
                    else
                        item.insertText = name + parenthesesSnippet;
                }

                // Add documentation
                if (!entry.documentationSymbol && ftv->definition && ftv->definition->definitionModuleName)
                {
                    item.documentation = {lsp::MarkupKind::Markdown,
                        printMoonwaveDocumentation(getComments(ftv->definition->definitionModuleName.value(), ftv->definition->definitionLocation))};
                }
            }
            else if (auto ttv = Luau::get<Luau::TableTypeVar>(id))
            {
                // Special case the RBXScriptSignal type as a connection
                if (ttv->name && ttv->name.value() == "RBXScriptSignal")
                {
                    item.kind = lsp::CompletionItemKind::Event;
                }
            }
            else if (Luau::get<Luau::ClassTypeVar>(id))
            {
                item.kind = lsp::CompletionItemKind::Class;
            }
            item.detail = Luau::toString(id);
        }

        items.emplace_back(item);
    }

    if (config.completion.suggestImports &&
        (result.context == Luau::AutocompleteContext::Expression || result.context == Luau::AutocompleteContext::Statement))
    {
        suggestImports(moduleName, position, config, items);
    }

    return items;
}

std::vector<lsp::CompletionItem> LanguageServer::completion(const lsp::CompletionParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->completion(params);
}
