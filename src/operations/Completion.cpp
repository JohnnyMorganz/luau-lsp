#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/Protocol.hpp"
#include "LSP/LuauExt.hpp"

static std::optional<Luau::AutocompleteEntryMap> nullCallback(std::string tag, std::optional<const Luau::ClassTypeVar*> ptr)
{
    return std::nullopt;
}

void WorkspaceFolder::endAutocompletion(const lsp::CompletionParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto position = convertPosition(params.position);

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
        if (!fileResolver.isManagedFile(moduleName))
            return;
        auto document = fileResolver.managedFiles.at(moduleName);
        auto lines = document.getLines();

        // If the position marker is at the very end of the file, if we insert one line further then vscode will
        // not be happy and will insert at the position marker.
        // If its in the middle of the file, vscode won't change the marker
        if (params.position.line == lines.size() - 1)
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
            if (lines.size() > 1)
            {
                // Use the indentation of the previous line, as thats where the stat begins
                auto prevLine = lines.at(params.position.line - 1);
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

std::vector<lsp::CompletionItem> WorkspaceFolder::completion(const lsp::CompletionParams& params)
{
    if (params.context && params.context->triggerCharacter == "\n")
    {
        if (client->getConfiguration(rootUri).autocompleteEnd)
            endAutocompletion(params);
        return {};
    }

    auto result = Luau::autocomplete(frontend, fileResolver.getModuleName(params.textDocument.uri), convertPosition(params.position), nullCallback);
    std::vector<lsp::CompletionItem> items;

    for (auto& [name, entry] : result.entryMap)
    {
        lsp::CompletionItem item;
        item.label = name;
        item.deprecated = entry.deprecated;

        if (entry.documentationSymbol)
            item.documentation = {lsp::MarkupKind::Markdown, printDocumentation(client->documentation, *entry.documentationSymbol)};

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

        // Handle if name is a property with spaces
        if (entry.kind == Luau::AutocompleteEntryKind::Property && name.find(" ") != std::string::npos)
        {
            auto lastAst = result.ancestry.back();
            if (auto indexName = lastAst->as<Luau::AstExprIndexName>())
            {
                lsp::TextEdit textEdit;
                textEdit.newText = "[\"" + name + "\"]";
                textEdit.range = {convertPosition(indexName->indexLocation.begin), convertPosition(indexName->indexLocation.end)};
                item.textEdit = textEdit;

                // For some reason, the above text edit can't handle replacing the index operator
                // Hence we remove it using an additional text edit
                item.additionalTextEdits.emplace_back(
                    lsp::TextEdit{{convertPosition(indexName->opPosition), {indexName->opPosition.line, indexName->opPosition.column + 1}}, ""});
            }
        }

        // Handle parentheses suggestions
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
            if (item.textEdit)
                item.textEdit->newText += "($1)$0";
            else
                item.insertText = name + "($1)$0";
            item.insertTextFormat = lsp::InsertTextFormat::Snippet;
            // Trigger Signature Help
            item.command = lsp::Command{"Trigger Signature Help", "editor.action.triggerParameterHints"};
        }

        if (entry.type.has_value())
        {
            auto id = Luau::follow(entry.type.value());
            // Try to infer more type info about the entry to provide better suggestion info
            if (auto ftv = Luau::get<Luau::FunctionTypeVar>(id))
            {
                item.kind = lsp::CompletionItemKind::Function;

                // Add label details
                std::string detail = "(";
                bool comma = false;
                for (auto arg : ftv->argNames)
                {
                    if (comma)
                        detail += ", ";
                    detail += arg.has_value() ? arg->name : "_";
                }
                detail += ")";
                item.labelDetails = {detail};
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

    return items;
}

std::vector<lsp::CompletionItem> LanguageServer::completion(const lsp::CompletionParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->completion(params);
}
