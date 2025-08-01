#include <unordered_set>
#include <utility>

#include "Luau/AstQuery.h"
#include "Luau/Autocomplete.h"
#include "Luau/FragmentAutocomplete.h"
#include "Luau/TxnLog.h"
#include "Luau/TypeUtils.h"
#include "Luau/TimeTrace.h"

#include "LSP/Completion.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/LuauExt.hpp"
#include "LSP/DocumentationParser.hpp"

LUAU_FASTFLAG(LuauSolverV2)

static Luau::AstNode* getParentNode(const std::vector<Luau::AstNode*> ancestry)
{
    if (ancestry.size() < 2)
        return nullptr;
    for (auto it = ++ancestry.rbegin(); it != ancestry.rend(); ++it)
    {
        if ((*it)->is<Luau::AstExprError>() || (*it)->is<Luau::AstStatError>())
            continue;
        return *it;
    }
    return nullptr;
}

void WorkspaceFolder::endAutocompletion(const lsp::CompletionParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto document = fileResolver.getTextDocument(params.textDocument.uri);
    if (!document)
        return;
    auto position = document->convertPosition(params.position);

    frontend.parse(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return;

    auto ancestry = Luau::findAstAncestryOfPosition(*sourceModule, position);
    if (ancestry.size() < 2)
        return;

    // Remove error nodes from end of ancestry chain
    while (ancestry.size() > 0 && (ancestry.back()->is<Luau::AstStatError>() || ancestry.back()->is<Luau::AstExprError>()))
        ancestry.pop_back();

    if (ancestry.size() == 0)
        return;

    Luau::AstNode* currentNode = ancestry.back();

    // TODO: https://github.com/luau-lang/luau/issues/1328 causes the ast ancestry to be shorter than expected
    if (auto globalFunc = currentNode->as<Luau::AstStatFunction>())
    {
        ancestry.push_back(globalFunc->func);
        ancestry.push_back(globalFunc->func->body);
        currentNode = globalFunc->func->body;
    }

    // We should only apply it if the line just above us is the start of the unclosed statement
    // Otherwise, we insert ends in weird places if theirs an unclosed stat a while away
    if (!currentNode->is<Luau::AstStatBlock>())
        return;
    if (params.position.line - currentNode->location.begin.line > 1)
        return;

    auto unclosedBlock = false;
    for (auto it = ancestry.rbegin(); it != ancestry.rend(); ++it)
    {
        if (auto* statForIn = (*it)->as<Luau::AstStatForIn>(); statForIn && !statForIn->body->hasEnd)
            unclosedBlock = true;
        else if (auto* statFor = (*it)->as<Luau::AstStatFor>(); statFor && !statFor->body->hasEnd)
            unclosedBlock = true;
        else if (auto* statIf = (*it)->as<Luau::AstStatIf>())
        {
            bool hasEnd = statIf->thenbody->hasEnd;
            if (statIf->elsebody)
            {
                if (auto* elseBlock = statIf->elsebody->as<Luau::AstStatBlock>())
                    hasEnd = elseBlock->hasEnd;
            }

            if (!hasEnd)
                unclosedBlock = true;
        }
        else if (auto* statWhile = (*it)->as<Luau::AstStatWhile>(); statWhile && !statWhile->body->hasEnd)
            unclosedBlock = true;
        else if (auto* exprFunction = (*it)->as<Luau::AstExprFunction>(); exprFunction && !exprFunction->body->hasEnd)
            unclosedBlock = true;
        if (auto* exprBlock = (*it)->as<Luau::AstStatBlock>(); exprBlock && !exprBlock->hasEnd)
            unclosedBlock = true;

        // FIX: if the unclosedBlock came from a repeat, then don't autocomplete, as it will be wrong!
        if (auto* statRepeat = (*it)->as<Luau::AstStatRepeat>(); statRepeat && !statRepeat->body->hasEnd)
            unclosedBlock = false;
    }

    std::vector<lsp::TextEdit> edits;
    bool moveCursorUp = false;

    // TODO: handle `until` for repeat: `until` can be inserted if `hasEnd` in a repeat block is false

    auto parentNode = getParentNode(ancestry);
    if (parentNode)
    {
        if (auto* statIf = parentNode->as<Luau::AstStatIf>(); statIf && statIf->condition && !statIf->thenLocation)
        {
            lsp::Position thenLocation = document->convertPosition(statIf->thenbody->location.begin);
            edits.emplace_back(lsp::TextEdit{{thenLocation, thenLocation}, " then"});
        }
        else if (auto* statWhile = parentNode->as<Luau::AstStatWhile>(); statWhile && statWhile->condition && !statWhile->hasDo)
        {
            lsp::Position doLocation = document->convertPosition(statWhile->body->location.begin);
            edits.emplace_back(lsp::TextEdit{{doLocation, doLocation}, " do"});
        }
        else if (auto* statForIn = parentNode->as<Luau::AstStatForIn>(); statForIn && statForIn->values.size > 0 && !statForIn->hasDo)
        {
            lsp::Position doLocation = document->convertPosition(statForIn->body->location.begin);
            edits.emplace_back(lsp::TextEdit{{doLocation, doLocation}, " do"});
        }
        else if (auto* statFor = parentNode->as<Luau::AstStatFor>(); statFor && !statFor->hasDo)
        {
            lsp::Position doLocation = document->convertPosition(statFor->body->location.begin);
            edits.emplace_back(lsp::TextEdit{{doLocation, doLocation}, " do"});
        }
    }

    if (unclosedBlock)
    {
        // Take into account the current line content when inserting end
        // in case we are e.g. inside of a function call
        auto currentLineContent = document->getLine(params.position.line);
        trim(currentLineContent);

        // Compute the current indentation level
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

        // TODO: it would be nicer if we had snippet support, and could insert text *after* the cursor
        // and leave the cursor in the same spot. Right now we can only insert text *at* the cursor,
        // then have to manually send a command to move the cursor
        // If we have content already on the current line, we cannot "replace" it whilst also
        // putting the end on the line afterwards, so we fallback to the manual movement method

        // If the position marker is at the very end of the file, if we insert one line further then vscode will
        // not be happy and will insert at the position marker.
        // If its in the middle of the file, vscode won't change the marker
        if (params.position.line == document->lineCount() - 1 || !currentLineContent.empty())
        {
            // Insert an end at the current position, with a newline before it
            auto insertText = "\n" + indent + "end" + currentLineContent + "\n";
            edits.emplace_back(lsp::TextEdit{{{params.position.line, 0}, {params.position.line + 1, 0}}, insertText});
            moveCursorUp = true;
        }
        else
        {
            LUAU_ASSERT(currentLineContent.empty());

            // Insert the end onto the next line
            lsp::Position position{params.position.line + 1, 0};
            edits.emplace_back(lsp::TextEdit{{position, position}, indent + "end\n"});
        }
    }

    if (!edits.empty())
    {
        std::unordered_map<Uri, std::vector<lsp::TextEdit>, UriHash> changes{{params.textDocument.uri, edits}};
        if (moveCursorUp)
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
        else
            client->applyEdit({"insert end", {changes}});
    }
}

void WorkspaceFolder::suggestImports(const Luau::ModuleName& moduleName, const Luau::Position& position, const ClientConfiguration& config,
    const TextDocument& textDocument, std::vector<lsp::CompletionItem>& result, bool completingTypeReferencePrefix)
{
    LUAU_TIMETRACE_SCOPE("WorkspaceFolder::suggestImports", "LSP");
    auto sourceModule = frontend.getSourceModule(moduleName);
    auto module = getModule(moduleName, /* forAutocomplete: */ true);
    if (!sourceModule || !module)
        return;

    auto scope = Luau::findScopeAtPosition(*module, position);
    if (!scope)
        return;

    // Place after any hot comments
    size_t hotCommentsLineNumber = 0;
    for (const auto& hotComment : sourceModule->hotcomments)
    {
        if (!hotComment.header)
            continue;
        if (hotComment.location.begin.line >= hotCommentsLineNumber)
            hotCommentsLineNumber = hotComment.location.begin.line + 1U;
    }

    platform->handleSuggestImports(textDocument, *sourceModule, config, hotCommentsLineNumber, completingTypeReferencePrefix, result);
}

static bool canUseSnippets(const lsp::ClientCapabilities& capabilities)
{
    return capabilities.textDocument && capabilities.textDocument->completion && capabilities.textDocument->completion->completionItem &&
           capabilities.textDocument->completion->completionItem->snippetSupport;
}

static bool deprecated(const Luau::AutocompleteEntry& entry, std::optional<lsp::MarkupContent> documentation)
{
    if (entry.deprecated)
        return true;

    if (documentation)
        if (documentation->value.find("@deprecated") != std::string::npos)
            return true;

    return false;
}

static std::optional<lsp::CompletionItemKind> entryKind(const std::string& label, const Luau::AutocompleteEntry& entry, LSPPlatform* platform)
{
    if (auto kind = platform->handleEntryKind(entry))
        return kind;

    if (entry.type.has_value() && entry.kind != Luau::AutocompleteEntryKind::Type)
    {
        auto id = Luau::follow(entry.type.value());
        if (Luau::isOverloadedFunction(id))
            return lsp::CompletionItemKind::Function;

        // Try to infer more type info about the entry to provide better suggestion info
        if (Luau::get<Luau::FunctionType>(id))
            return lsp::CompletionItemKind::Function;
    }

    if (std::find(entry.tags.begin(), entry.tags.end(), "Alias") != entry.tags.end())
        return lsp::CompletionItemKind::Constant;
    else if (std::find(entry.tags.begin(), entry.tags.end(), "File") != entry.tags.end())
        return lsp::CompletionItemKind::File;
    else if (std::find(entry.tags.begin(), entry.tags.end(), "Directory") != entry.tags.end())
        return lsp::CompletionItemKind::Folder;

    switch (entry.kind)
    {
    case Luau::AutocompleteEntryKind::Property:
        return lsp::CompletionItemKind::Field;
    case Luau::AutocompleteEntryKind::Binding:
        return lsp::CompletionItemKind::Variable;
    case Luau::AutocompleteEntryKind::Keyword:
        return lsp::CompletionItemKind::Keyword;
    case Luau::AutocompleteEntryKind::String:
        return lsp::CompletionItemKind::Constant;
    case Luau::AutocompleteEntryKind::Type:
        return lsp::CompletionItemKind::Interface;
    case Luau::AutocompleteEntryKind::Module:
        return lsp::CompletionItemKind::Module;
    case Luau::AutocompleteEntryKind::GeneratedFunction:
        return lsp::CompletionItemKind::Function;
    case Luau::AutocompleteEntryKind::RequirePath:
    {
        if (label == ".." || label == ".")
            return lsp::CompletionItemKind::Folder;
        return lsp::CompletionItemKind::File;
    }
    }

    return std::nullopt;
}

static const char* sortText(const Luau::Frontend& frontend, const std::string& name, const Luau::AutocompleteEntry& entry,
    const std::unordered_set<std::string>& tags, LSPPlatform& platform)
{
    if (auto text = platform.handleSortText(frontend, name, entry, tags))
        return text;

    // If it's a file or directory alias, de-prioritise it compared to normal paths
    if (std::find(entry.tags.begin(), entry.tags.end(), "Alias") != entry.tags.end())
        return SortText::AutoImports;

    // If the entry is `loadstring`, deprioritise it
    auto& completionGlobals = FFlag::LuauSolverV2 ? frontend.globals : frontend.globalsForAutocomplete;
    if (auto it = completionGlobals.globalScope->bindings.find(Luau::AstName("loadstring")); it != completionGlobals.globalScope->bindings.end())
    {
        if (entry.type == it->second.typeId)
            return SortText::Deprioritized;
    }

    if (entry.wrongIndexType)
        return SortText::WrongIndexType;
    if (entry.typeCorrect == Luau::TypeCorrectKind::Correct)
        return SortText::CorrectTypeKind;
    else if (entry.typeCorrect == Luau::TypeCorrectKind::CorrectFunctionResult)
        return SortText::CorrectFunctionResult;
    else if (entry.kind == Luau::AutocompleteEntryKind::Property && types::isMetamethod(name))
        return SortText::MetatableIndex;
    else if (entry.kind == Luau::AutocompleteEntryKind::Property)
        return SortText::TableProperties;
    else if (entry.kind == Luau::AutocompleteEntryKind::Keyword)
        return SortText::Keywords;

    return SortText::Default;
}

const std::vector<std::string> keywords = {"and", "break", "do", "else", "elseif", "end", "false", "for", "function", "if", "in", "local", "nil",
    "not", "or", "repeat", "return", "then", "true", "until", "while"};

static bool isKeyword(std::string_view s)
{
    return std::find(keywords.begin(), keywords.end(), s) != keywords.end();
}

static bool isIdentifier(std::string_view s)
{
    return Luau::isIdentifier(s) && !isKeyword(s);
}

static std::pair<std::string, std::string> computeLabelDetailsForFunction(const Luau::AutocompleteEntry& entry, const Luau::FunctionType* ftv)
{
    std::string detail = "(";
    std::string parenthesesSnippet = "(";

    bool comma = false;
    size_t argIndex = 0;
    size_t snippetIndex = 1;

    auto [minCount, _] = Luau::getParameterExtents(Luau::TxnLog::empty(), ftv->argTypes, true);

    // Include 'unknown' arguments as required types
    for (auto arg : ftv->argTypes)
        if (Luau::get<Luau::UnknownType>(follow(arg)))
            minCount += 1;

    auto it = Luau::begin(ftv->argTypes);
    for (; it != Luau::end(ftv->argTypes); ++it, ++argIndex)
    {
        std::string argName = "_";
        if (argIndex < ftv->argNames.size() && ftv->argNames.at(argIndex))
            argName = ftv->argNames.at(argIndex)->name;

        if (argIndex == 0 && entry.indexedWithSelf)
            continue;

        // If the rest of the arguments are optional, don't include in filled call arguments
        bool includeParensSnippet = argIndex < minCount;

        if (comma)
        {
            detail += ", ";
            if (includeParensSnippet)
                parenthesesSnippet += ", ";
        }

        detail += argName;
        if (includeParensSnippet)
            parenthesesSnippet += "${" + std::to_string(snippetIndex) + ":" + argName + "}";

        comma = true;
        snippetIndex++;
    }

    if (auto tail = it.tail())
    {
        if (comma)
        {
            detail += ", ";
        }
        detail += Luau::toString(*tail);
    }

    // If Luau recommended we put the cursor inside, but we haven't recorded any arguments yet, then we are going to fail to do this.
    // This can happen when all the arguments to function are optional or any (e.g., wait or require)
    // Let's force a tabstop inside if this happens
    if (entry.parens == Luau::ParenthesesRecommendation::CursorInside && parenthesesSnippet == "(")
    {
        parenthesesSnippet += "$1";
    }

    detail += ")";
    parenthesesSnippet += ")";

    return std::make_pair(detail, parenthesesSnippet);
}

std::optional<std::string> WorkspaceFolder::getDocumentationForAutocompleteEntry(
    const std::string& name, const Luau::AutocompleteEntry& entry, const std::vector<Luau::AstNode*>& ancestry, const Luau::ModulePtr& localModule)
{
    if (entry.documentationSymbol)
        if (auto docs = printDocumentation(client->documentation, *entry.documentationSymbol))
            return docs;

    if (entry.type.has_value())
        if (auto documentation = getDocumentationForType(entry.type.value()))
            return documentation;

    if (entry.prop)
    {
        std::optional<Luau::ModuleName> definitionModuleName;

        if (entry.containingExternType)
        {
            definitionModuleName = entry.containingExternType.value()->definitionModuleName;
        }
        else
        {
            // TODO: there is not a nice way to get the containing table type from the entry, so we compute it ourselves
            if (localModule)
            {
                Luau::TypeId* parentTy = nullptr;
                if (auto node = ancestry.back())
                {
                    if (auto indexName = node->as<Luau::AstExprIndexName>())
                        parentTy = localModule->astTypes.find(indexName->expr);
                    else if (auto indexExpr = node->as<Luau::AstExprIndexExpr>())
                        parentTy = localModule->astTypes.find(indexExpr->expr);
                }

                if (parentTy)
                {
                    // parentTy might be an intersected type, find the actual base ttv
                    auto followedTy = Luau::follow(*parentTy);
                    if (auto propInformation = lookupProp(followedTy, name))
                        definitionModuleName = Luau::getDefinitionModuleName(propInformation->first);
                    else
                        definitionModuleName = Luau::getDefinitionModuleName(followedTy);
                }
            }
        }

        if (definitionModuleName)
        {
            if (auto propLocation = entry.prop.value()->location)
                if (auto text = printMoonwaveDocumentation(getComments(definitionModuleName.value(), propLocation.value())); !text.empty())
                    return text;

            if (auto typeLocation = entry.prop.value()->typeLocation)
                if (auto text = printMoonwaveDocumentation(getComments(definitionModuleName.value(), typeLocation.value())); !text.empty())
                    return text;
        }
    }

    return std::nullopt;
}

std::vector<lsp::CompletionItem> WorkspaceFolder::completion(const lsp::CompletionParams& params, const LSPCancellationToken& cancellationToken)
{
    LUAU_TIMETRACE_SCOPE("WorkspaceFolder::completion", "LSP");
    auto config = client->getConfiguration(rootUri);

    if (!config.completion.enabled)
        return {};

    if (params.context && params.context->triggerCharacter == "\n")
    {
        if (config.autocompleteEnd || config.completion.autocompleteEnd)
            endAutocompletion(params);
        return {};
    }

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());

    std::unordered_set<std::string> tags;

    auto stringCompletionCB = [&](const std::string& tag, std::optional<const Luau::ExternType*> ctx,
                                  std::optional<std::string> contents) -> std::optional<Luau::AutocompleteEntryMap>
    {
        tags.insert(tag);
        return platform->completionCallback(tag, ctx, std::move(contents), moduleName);
    };

    auto position = textDocument->convertPosition(params.position);

    Luau::FragmentAutocompleteStatusResult fragmentStatusResult;
    Luau::AutocompleteResult result;
    bool forAutocomplete = !FFlag::LuauSolverV2; // New type solver does not have a different engine for autocomplete
    bool fragmentWasSuccessful = false;

    if (config.completion.enableFragmentAutocomplete && frontend.allModuleDependenciesValid(moduleName, forAutocomplete) &&
        frontend.isDirty(moduleName, forAutocomplete))
    {
        Luau::FrontendOptions frontendOptions;
        frontendOptions.retainFullTypeGraphs = true;
        if (FFlag::LuauSolverV2)
            frontendOptions.runLintChecks = true;
        else
            frontendOptions.forAutocomplete = true;
        frontendOptions.cancellationToken = cancellationToken;

        // Get parse information for this script
        frontend.parse(moduleName);
        const auto sourceModule = frontend.getSourceModule(moduleName);
        if (!sourceModule)
            return {};

        auto newSrc = textDocument->getText();
        Luau::FragmentContext fragmentContext = {
            newSrc,
            // TODO: we have to construct a parse result as tryFragmentAutocomplete only accepts this
            Luau::ParseResult{sourceModule->root},
            frontendOptions,
        };

        // It is important to keep the fragmentResult in scope for the whole completion step
        // Otherwise the incremental module may de-allocate leading to a use-after-free when accessing the result ancestry
        fragmentStatusResult = Luau::tryFragmentAutocomplete(frontend, moduleName, position, fragmentContext, stringCompletionCB);
        throwIfCancelled(cancellationToken);
        if (fragmentStatusResult.status == Luau::FragmentAutocompleteStatus::Success)
        {
            // Result is nullopt if there are no suggestions (i.e. comments)
            if (!fragmentStatusResult.result)
                return {};

            result = fragmentStatusResult.result->acResults;
            fragmentWasSuccessful = true;
        }
    }

    if (!fragmentWasSuccessful)
    {
        // We must perform check before autocompletion
        checkStrict(moduleName, cancellationToken, forAutocomplete);

        throwIfCancelled(cancellationToken);

        result = Luau::autocomplete(frontend, moduleName, position, stringCompletionCB);
    }

    std::vector<lsp::CompletionItem> items{};

    for (auto& [name, entry] : result.entryMap)
    {
        // If this entry is a non-function property, and we are autocompleting after a `:`
        // then hide it if configured to do so
        if (!config.completion.showPropertiesOnMethodCall && entry.kind == Luau::AutocompleteEntryKind::Property && entry.indexedWithSelf &&
            !(Luau::get<Luau::FunctionType>(*entry.type) || Luau::isOverloadedFunction(*entry.type)))
            continue;

        lsp::CompletionItem item;
        item.label = name;

        // Remove the trailing slash in `../` and `./` as it prevents completion from triggering
        if (entry.kind == Luau::AutocompleteEntryKind::RequirePath)
        {
            if (name == "../")
                item.label = "..";
            else if (name == "./")
                item.label = ".";
        }

        const auto localModule = fragmentWasSuccessful ? fragmentStatusResult.result->incrementalModule : getModule(moduleName, forAutocomplete);
        if (auto documentationString = getDocumentationForAutocompleteEntry(name, entry, result.ancestry, localModule))
            item.documentation = {lsp::MarkupKind::Markdown, documentationString.value()};

        item.deprecated = deprecated(entry, item.documentation);
        item.kind = entryKind(item.label, entry, platform.get());
        item.sortText = sortText(frontend, item.label, entry, tags, *platform);

        if (entry.kind == Luau::AutocompleteEntryKind::GeneratedFunction)
            item.insertText = entry.insertText;

        if (entry.kind == Luau::AutocompleteEntryKind::RequirePath)
        {
            if (entry.insertText)
            {
                LUAU_ASSERT(!result.ancestry.empty());
                auto containingString = result.ancestry.back()->as<Luau::AstExprConstantString>();
                LUAU_ASSERT(containingString);

                auto insertText = entry.insertText;
                if (!insertText->empty() && insertText->back() == '/')
                    insertText->pop_back();

                item.textEdit = lsp::TextEdit{{params.position, textDocument->convertPosition(Luau::Position{
                                                                    containingString->location.end.line, containingString->location.end.column - 1})},
                    *insertText};

                // TextEdit cannot replace the old text for some reason, so we need an additional edit
                item.additionalTextEdits.emplace_back(
                    lsp::TextEdit{{textDocument->convertPosition(
                                       Luau::Position{containingString->location.begin.line, containingString->location.begin.column + 1}),
                                      textDocument->convertPosition(
                                          Luau::Position{containingString->location.end.line, containingString->location.end.column - 1})},
                        ""});
            }
        }

        // Handle if name is not an identifier
        if (entry.kind == Luau::AutocompleteEntryKind::Property && !isIdentifier(name))
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

        // If autocompleting in a string and the autocompleting text contains a '/' character, then it won't replace correctly due to word boundaries
        // Apply a complete text edit instead
        if (name.find('/') != std::string::npos && result.context == Luau::AutocompleteContext::String &&
            entry.kind != Luau::AutocompleteEntryKind::RequirePath)
        {
            auto lastAst = result.ancestry.back();
            if (auto str = lastAst->as<Luau::AstExprConstantString>())
            {
                lsp::TextEdit textEdit;
                textEdit.newText = name;
                // Range is inside the quotes
                textEdit.range = {textDocument->convertPosition(Luau::Position{str->location.begin.line, str->location.begin.column + 1}),
                    textDocument->convertPosition(Luau::Position{str->location.end.line, str->location.end.column - 1})};
                item.textEdit = textEdit;
            }
        }

        // Handle parentheses suggestions
        if (config.completion.addParentheses)
        {
            if (canUseSnippets(client->capabilities))
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
            else
            {
                // We don't support snippets, so just add parentheses
                if (entry.parens == Luau::ParenthesesRecommendation::CursorAfter || entry.parens == Luau::ParenthesesRecommendation::CursorInside)
                {
                    if (item.textEdit)
                        item.textEdit->newText += "()";
                    else
                        item.insertText = name + "()";
                }
            }
        }

        if (entry.type.has_value())
        {
            auto id = Luau::follow(entry.type.value());
            item.detail = Luau::toString(id);

            // Try to infer more type info about the entry to provide better suggestion info
            if (auto ftv = Luau::get<Luau::FunctionType>(id);
                ftv && entry.kind != Luau::AutocompleteEntryKind::GeneratedFunction && entry.kind != Luau::AutocompleteEntryKind::Type)
            {
                // Compute label details and more detailed parentheses snippet
                auto [detail, parenthesesSnippet] = computeLabelDetailsForFunction(entry, ftv);
                item.labelDetails = {detail};

                // If we had CursorAfter, then the function call would not have any arguments
                if (canUseSnippets(client->capabilities) && config.completion.addParentheses && config.completion.fillCallArguments &&
                    entry.parens != Luau::ParenthesesRecommendation::None)
                {
                    if (config.completion.addTabstopAfterParentheses)
                        parenthesesSnippet += "$0";

                    if (item.textEdit)
                        item.textEdit->newText += parenthesesSnippet;
                    else
                        item.insertText = name + parenthesesSnippet;

                    item.insertTextFormat = lsp::InsertTextFormat::Snippet;
                    item.command = lsp::Command{"Trigger Signature Help", "editor.action.triggerParameterHints"};
                }
            }
        }

        items.emplace_back(item);
    }

    if (auto module = frontend.getSourceModule(moduleName))
        platform->handleCompletion(*textDocument, *module, position, items);

    if (config.completion.suggestImports || config.completion.imports.enabled)
    {
        if (result.context == Luau::AutocompleteContext::Expression || result.context == Luau::AutocompleteContext::Statement)
        {
            suggestImports(moduleName, position, config, *textDocument, items, /* completingTypeReferencePrefix: */ false);
        }
        else if (result.context == Luau::AutocompleteContext::Type)
        {
            // Make sure we are in the context of completing a prefix in an AstTypeReference
            if (auto node = result.ancestry.back())
                if (auto typeReference = node->as<Luau::AstTypeReference>())
                    if (!typeReference->prefix)
                        suggestImports(moduleName, position, config, *textDocument, items, /* completingTypeReferencePrefix: */ true);
        }
    }

    return items;
}
