#include <unordered_set>
#include <utility>

#include "Luau/AstQuery.h"
#include "Luau/Autocomplete.h"
#include "Luau/TypeUtils.h"

#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/LuauExt.hpp"
#include "LSP/DocumentationParser.hpp"

LUAU_FASTFLAG(LuauClipExtraHasEndProps);
LUAU_FASTFLAG(LuauAutocompleteDoEnd);

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

    // Search backwards for the first node that is not an Error node
    size_t currentNodeIndex = ancestry.size() - 1;
    while (ancestry.at(currentNodeIndex)->is<Luau::AstStatError>() || ancestry.at(currentNodeIndex)->is<Luau::AstExprError>())
    {
        currentNodeIndex--;
        if (currentNodeIndex < 1)
            return;
    }

    Luau::AstNode* currentNode = ancestry.at(currentNodeIndex);
    if (!currentNode)
        return;

    // We should only apply it if the line just above us is the start of the unclosed statement
    // Otherwise, we insert ends in weird places if theirs an unclosed stat a while away
    if (!currentNode->is<Luau::AstStatBlock>())
        return;
    if (params.position.line - currentNode->location.begin.line > 1)
        return;

    auto unclosedBlock = false;
    if (FFlag::LuauClipExtraHasEndProps)
    {
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
            if (FFlag::LuauAutocompleteDoEnd)
            {
                if (auto* exprBlock = (*it)->as<Luau::AstStatBlock>(); exprBlock && !exprBlock->hasEnd)
                    unclosedBlock = true;

                // FIX: if the unclosedBlock came from a repeat, then don't autocomplete, as it will be wrong!
                if (auto* statRepeat = (*it)->as<Luau::AstStatRepeat>(); statRepeat && !statRepeat->body->hasEnd)
                    unclosedBlock = false;
            }
        }
    }
    else
    {
        for (auto it = ancestry.rbegin(); it != ancestry.rend(); ++it)
        {
            if (auto* statForIn = (*it)->as<Luau::AstStatForIn>(); statForIn && !statForIn->DEPRECATED_hasEnd)
                unclosedBlock = true;
            if (auto* statFor = (*it)->as<Luau::AstStatFor>(); statFor && !statFor->DEPRECATED_hasEnd)
                unclosedBlock = true;
            if (auto* statIf = (*it)->as<Luau::AstStatIf>(); statIf && !statIf->DEPRECATED_hasEnd)
                unclosedBlock = true;
            if (auto* statWhile = (*it)->as<Luau::AstStatWhile>(); statWhile && !statWhile->DEPRECATED_hasEnd)
                unclosedBlock = true;
            if (FFlag::LuauAutocompleteDoEnd)
            {
                if (auto* statBlock = (*it)->as<Luau::AstStatBlock>(); statBlock && !statBlock->hasEnd)
                    unclosedBlock = true;
            }
            if (auto* exprFunction = (*it)->as<Luau::AstExprFunction>(); exprFunction && !exprFunction->DEPRECATED_hasEnd)
                unclosedBlock = true;
        }
    }

    // TODO: we could potentially extend this further that just `hasEnd`
    // by inserting `then`, `until` `do` etc. It seems Studio does this
    // NOTE: `until` can be inserted if `hasEnd` in a repeat block is false

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

            lsp::TextEdit edit{{{params.position.line, 0}, {params.position.line + 1, 0}}, insertText};
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
            LUAU_ASSERT(currentLineContent.empty());

            // Insert the end onto the next line
            lsp::Position position{params.position.line + 1, 0};
            lsp::TextEdit edit{{position, position}, indent + "end\n"};
            std::unordered_map<std::string, std::vector<lsp::TextEdit>> changes{{params.textDocument.uri.toString(), {edit}}};
            client->applyEdit({"insert end", {changes}});
        }
    }
}

static lsp::TextEdit createRequireTextEdit(const std::string& name, const std::string& path, size_t lineNumber, bool prependNewline = false)
{
    auto range = lsp::Range{{lineNumber, 0}, {lineNumber, 0}};
    auto importText = "local " + name + " = require(" + path + ")\n";
    if (prependNewline)
        importText = "\n" + importText;
    return {range, importText};
}

static lsp::CompletionItem createSuggestRequire(
    const std::string& name, const std::vector<lsp::TextEdit>& textEdits, const char* sortText, const std::string& path)
{
    std::string documentation;
    for (const auto& edit : textEdits)
        documentation += edit.newText;

    lsp::CompletionItem item;
    item.label = name;
    item.kind = lsp::CompletionItemKind::Module;
    item.detail = "Auto-import";
    item.documentation = {lsp::MarkupKind::Markdown, codeBlock("luau", documentation) + "\n\n" + path};
    item.insertText = name;
    item.sortText = sortText;

    item.additionalTextEdits = textEdits;

    return item;
}

static size_t getLengthEqual(const std::string& a, const std::string& b)
{
    size_t i = 0;
    for (; i < a.size() && i < b.size(); ++i)
    {
        if (a[i] != b[i])
            break;
    }
    return i;
}

static std::string optimiseAbsoluteRequire(const std::string& path)
{
    if (!Luau::startsWith(path, "game/"))
        return path;

    auto parts = Luau::split(path, '/');
    if (parts.size() > 2)
    {
        auto service = std::string(parts[1]);
        return service + "/" + Luau::join(std::vector(parts.begin() + 2, parts.end()), "/");
    }

    return path;
}

void WorkspaceFolder::suggestImports(const Luau::ModuleName& moduleName, const Luau::Position& position, const ClientConfiguration& config,
    const TextDocument& textDocument, std::vector<lsp::CompletionItem>& result, bool isType)
{
    auto sourceModule = frontend.getSourceModule(moduleName);
    auto module = frontend.moduleResolverForAutocomplete.getModule(moduleName);
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

    // Find all import calls
    std::unique_ptr<FindImportsVisitor> importsVisitor = platform->getImportVisitor();
    importsVisitor->visit(sourceModule->root);

    platform->handleSuggestImports(config, importsVisitor.get(), hotCommentsLineNumber, isType, result);

    if (config.completion.imports.suggestRequires)
    {
        size_t minimumLineNumber = hotCommentsLineNumber;
        size_t visitorMinimumLine = importsVisitor->getMinimumRequireLine();

        if (visitorMinimumLine > minimumLineNumber)
            minimumLineNumber = visitorMinimumLine;

        if (importsVisitor->firstRequireLine)
            minimumLineNumber = *importsVisitor->firstRequireLine >= minimumLineNumber ? (*importsVisitor->firstRequireLine) : minimumLineNumber;

        for (auto& [path, node] : fileResolver.virtualPathsToSourceNodes)
        {
            auto name = node->name;
            replaceAll(name, " ", "_");

            if (path == moduleName || node->className != "ModuleScript" || importsVisitor->containsRequire(name))
                continue;
            if (auto scriptFilePath = fileResolver.getRealPathFromSourceNode(node); scriptFilePath && isIgnoredFile(*scriptFilePath, config))
                continue;

            std::string requirePath;
            std::vector<lsp::TextEdit> textEdits;

            // Compute the style of require
            bool isRelative = false;
            auto parent1 = getParentPath(moduleName), parent2 = getParentPath(path);
            if (config.completion.imports.requireStyle == ImportRequireStyle::AlwaysRelative ||
                Luau::startsWith(path, "ProjectRoot/") || // All model projects should always require relatively
                (config.completion.imports.requireStyle != ImportRequireStyle::AlwaysAbsolute &&
                    (Luau::startsWith(moduleName, path) || Luau::startsWith(path, moduleName) || parent1 == parent2)))
            {
                requirePath = "./" + std::filesystem::relative(path, moduleName).string();
                isRelative = true;
            }
            else
                requirePath = optimiseAbsoluteRequire(path);

            auto require = convertToScriptPath(requirePath);

            size_t lineNumber = minimumLineNumber;
            size_t bestLength = 0;
            for (auto& group : importsVisitor->requiresMap)
            {
                for (auto& [_, stat] : group)
                {
                    auto line = stat->location.end.line;

                    // HACK: We read the text of the require argument to sort the lines
                    // Note: requires may be in the form `require(path) :: any`, so we need to handle that too
                    Luau::AstExprCall* call = stat->values.data[0]->as<Luau::AstExprCall>();
                    if (auto assertion = stat->values.data[0]->as<Luau::AstExprTypeAssertion>())
                        call = assertion->expr->as<Luau::AstExprCall>();
                    if (!call)
                        continue;

                    auto location = call->args.data[0]->location;
                    auto range = lsp::Range{{location.begin.line, location.begin.column}, {location.end.line, location.end.column}};
                    auto argText = textDocument.getText(range);
                    auto length = getLengthEqual(argText, require);

                    if (length > bestLength && argText < require && line >= lineNumber)
                        lineNumber = line + 1;
                }
            }

            platform->handleRequire(requirePath, lineNumber, isRelative, config, importsVisitor.get(), hotCommentsLineNumber, textEdits);

            // Whether we need to add a newline before the require to separate it from the services
            bool prependNewline = config.completion.imports.separateGroupsWithLine && importsVisitor->shouldPrependNewline(lineNumber);

            textEdits.emplace_back(createRequireTextEdit(node->name, require, lineNumber, prependNewline));

            result.emplace_back(createSuggestRequire(name, textEdits, isRelative ? SortText::AutoImports : SortText::AutoImportsAbsolute, path));
        }
    }
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

std::optional<lsp::CompletionItemKind> WorkspaceFolder::entryKind(const Luau::AutocompleteEntry& entry)
{
    if (auto kind = platform->handleEntryKind(entry))
        return kind;

    if (entry.type.has_value())
    {
        auto id = Luau::follow(entry.type.value());
        if (Luau::isOverloadedFunction(id))
            return lsp::CompletionItemKind::Function;

        // Try to infer more type info about the entry to provide better suggestion info
        if (Luau::get<Luau::FunctionType>(id))
            return lsp::CompletionItemKind::Function;
        else if (Luau::get<Luau::ClassType>(id))
            return lsp::CompletionItemKind::Class;
    }

    if (std::find(entry.tags.begin(), entry.tags.end(), "File") != entry.tags.end())
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
    if (auto it = frontend.globalsForAutocomplete.globalScope->bindings.find(Luau::AstName("loadstring"));
        it != frontend.globalsForAutocomplete.globalScope->bindings.end())
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

static std::pair<std::string, std::string> computeLabelDetailsForFunction(const Luau::AutocompleteEntry& entry, const Luau::FunctionType* ftv)
{
    std::string detail = "(";
    std::string parenthesesSnippet = "(";

    bool comma = false;
    size_t argIndex = 0;
    size_t snippetIndex = 1;

    auto [minCount, _] = Luau::getParameterExtents(Luau::TxnLog::empty(), ftv->argTypes, true);

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

    detail += ")";
    parenthesesSnippet += ")";

    return std::make_pair(detail, parenthesesSnippet);
}

std::optional<std::string> WorkspaceFolder::getDocumentationForAutocompleteEntry(
    const Luau::AutocompleteEntry& entry, const std::vector<Luau::AstNode*>& ancestry, const Luau::ModuleName& moduleName)
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

        if (entry.containingClass)
        {
            definitionModuleName = entry.containingClass.value()->definitionModuleName;
        }
        else
        {
            // TODO: there is not a nice way to get the containing table type from the entry, so we compute it ourselves
            auto module = frontend.moduleResolverForAutocomplete.getModule(moduleName);

            if (module)
            {
                Luau::TypeId* parentTy = nullptr;
                if (auto node = ancestry.back())
                {
                    if (auto indexName = node->as<Luau::AstExprIndexName>())
                        parentTy = module->astTypes.find(indexName->expr);
                    else if (auto indexExpr = node->as<Luau::AstExprIndexExpr>())
                        parentTy = module->astTypes.find(indexExpr->expr);
                }

                if (parentTy)
                    definitionModuleName = Luau::getDefinitionModuleName(*parentTy);
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

std::vector<lsp::CompletionItem> WorkspaceFolder::completion(const lsp::CompletionParams& params)
{
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

    // We must perform check before autocompletion
    checkStrict(moduleName, /* forAutocomplete: */ true);

    auto position = textDocument->convertPosition(params.position);
    auto result = Luau::autocomplete(frontend, moduleName, position,
        [&](const std::string& tag, std::optional<const Luau::ClassType*> ctx,
            std::optional<std::string> contents) -> std::optional<Luau::AutocompleteEntryMap>
        {
            tags.insert(tag);

            if (tag == "Require")
            {
                if (!contents.has_value())
                    return std::nullopt;

                auto config = client->getConfiguration(rootUri);

                Luau::AutocompleteEntryMap result;

                // Include any files in the directory
                auto contentsString = contents.value();

                // We should strip any trailing values until a `/` is found in case autocomplete
                // is triggered half-way through.
                // E.g., for "Contents/Test|", we should only consider up to "Contents/" to find all files
                // For "Mod|", we should only consider an empty string ""
                auto separator = contentsString.find_last_of("/\\");
                if (separator == std::string::npos)
                    contentsString = "";
                else
                    contentsString = contentsString.substr(0, separator + 1);

                // Populate with custom file aliases
                for (const auto& [aliasName, _] : config.require.fileAliases)
                {
                    Luau::AutocompleteEntry entry{
                        Luau::AutocompleteEntryKind::String, frontend.builtinTypes->stringType, false, false, Luau::TypeCorrectKind::Correct};
                    entry.tags.push_back("File");
                    entry.tags.push_back("Alias");
                    result.insert_or_assign(aliasName, entry);
                }

                // Populate with custom directory aliases, if we are at the start of a string require
                if (contentsString == "")
                {
                    for (const auto& [aliasName, _] : config.require.directoryAliases)
                    {
                        Luau::AutocompleteEntry entry{
                            Luau::AutocompleteEntryKind::String, frontend.builtinTypes->stringType, false, false, Luau::TypeCorrectKind::Correct};
                        entry.tags.push_back("Directory");
                        entry.tags.push_back("Alias");
                        result.insert_or_assign(aliasName, entry);
                    }
                }

                // Check if it starts with a directory alias, otherwise resolve with require base path
                std::filesystem::path currentDirectory = resolveDirectoryAlias(rootUri.fsPath(), config.require.directoryAliases, contentsString)
                                                             .value_or(fileResolver.getRequireBasePath(moduleName).append(contentsString));

                try
                {
                    for (const auto& dir_entry : std::filesystem::directory_iterator(currentDirectory))
                    {
                        if (dir_entry.is_regular_file() || dir_entry.is_directory())
                        {
                            std::string fileName = dir_entry.path().filename().generic_string();
                            Luau::AutocompleteEntry entry{
                                Luau::AutocompleteEntryKind::String, frontend.builtinTypes->stringType, false, false, Luau::TypeCorrectKind::Correct};
                            entry.tags.push_back(dir_entry.is_directory() ? "Directory" : "File");
                            result.insert_or_assign(fileName, entry);
                        }
                    }

                    // Add in ".." support
                    if (currentDirectory.has_parent_path())
                    {
                        Luau::AutocompleteEntry dotdotEntry{
                            Luau::AutocompleteEntryKind::String, frontend.builtinTypes->stringType, false, false, Luau::TypeCorrectKind::Correct};
                        dotdotEntry.tags.push_back("Directory");
                        result.insert_or_assign("..", dotdotEntry);
                    }
                }
                catch (std::exception&)
                {
                }

                return result;
            }

            return platform->completionCallback(tag, ctx, std::move(contents), moduleName);
        });


    std::vector<lsp::CompletionItem> items{};

    if (auto module = frontend.getSourceModule(moduleName))
        platform->handleCompletion(*textDocument, *module, position, items);

    for (auto& [name, entry] : result.entryMap)
    {
        lsp::CompletionItem item;
        item.label = name;

        if (auto documentationString = getDocumentationForAutocompleteEntry(entry, result.ancestry, moduleName))
            item.documentation = {lsp::MarkupKind::Markdown, documentationString.value()};

        item.deprecated = deprecated(entry, item.documentation);
        item.kind = entryKind(entry);
        item.sortText = sortText(frontend, name, entry, tags, *platform);

        if (entry.kind == Luau::AutocompleteEntryKind::GeneratedFunction)
            item.insertText = entry.insertText;

        // We shouldn't include the extension when inserting a file
        if (std::find(entry.tags.begin(), entry.tags.end(), "File") != entry.tags.end())
            if (auto pos = name.find_last_of('.'); pos != std::string::npos)
                item.insertText = std::string(name).erase(pos);

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
            if (auto ftv = Luau::get<Luau::FunctionType>(id); ftv && entry.kind != Luau::AutocompleteEntryKind::GeneratedFunction)
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

    if (config.completion.suggestImports || config.completion.imports.enabled)
    {
        if (result.context == Luau::AutocompleteContext::Expression || result.context == Luau::AutocompleteContext::Statement)
        {
            suggestImports(moduleName, position, config, *textDocument, items, /* isType: */ false);
        }
        else if (result.context == Luau::AutocompleteContext::Type)
        {
            // Make sure we are in the context of completing a prefix in an AstTypeReference
            if (auto node = result.ancestry.back())
                if (auto typeReference = node->as<Luau::AstTypeReference>())
                    if (!typeReference->prefix)
                        suggestImports(moduleName, position, config, *textDocument, items, /* isType: */ true);
        }
    }

    return items;
}

std::vector<lsp::CompletionItem> LanguageServer::completion(const lsp::CompletionParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->completion(params);
}
