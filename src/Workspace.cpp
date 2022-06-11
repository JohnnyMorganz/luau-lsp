#include <iostream>
#include <limits.h>
#include "LSP/Workspace.hpp"

static std::optional<Luau::AutocompleteEntryMap> nullCallback(std::string tag, std::optional<const Luau::ClassTypeVar*> ptr)
{
    return std::nullopt;
}


/// Checks whether a provided file is part of the workspace
bool WorkspaceFolder::isInWorkspace(const lsp::DocumentUri& file)
{
    // Check if the root uri is a prefix of the file
    auto prefixStr = rootUri.toString();
    auto checkStr = file.toString();
    if (checkStr.compare(0, prefixStr.size(), prefixStr) == 0)
    {
        return true;
    }
    return false;
}

void WorkspaceFolder::openTextDocument(const lsp::DocumentUri& uri, const lsp::DidOpenTextDocumentParams& params)
{
    auto moduleName = fileResolver.getModuleName(uri);
    fileResolver.managedFiles.emplace(
        std::make_pair(moduleName, TextDocument(uri, params.textDocument.languageId, params.textDocument.version, params.textDocument.text)));
    // Mark the file as dirty as we don't know what changes were made to it
    frontend.markDirty(moduleName);
}

void WorkspaceFolder::updateTextDocument(const lsp::DocumentUri& uri, const lsp::DidChangeTextDocumentParams& params)
{
    auto moduleName = fileResolver.getModuleName(uri);

    if (fileResolver.managedFiles.find(moduleName) == fileResolver.managedFiles.end())
    {
        std::cerr << "Text Document not loaded locally: " << uri.toString() << std::endl;
        return;
    }
    auto& textDocument = fileResolver.managedFiles.at(moduleName);
    textDocument.update(params.contentChanges, params.textDocument.version);

    // Mark the module dirty for the typechecker
    frontend.markDirty(moduleName);
}

void WorkspaceFolder::closeTextDocument(const lsp::DocumentUri& uri)
{
    auto moduleName = fileResolver.getModuleName(uri);
    fileResolver.managedFiles.erase(moduleName);
}

/// Whether the file has been marked as ignored by any of the ignored lists in the configuration
bool WorkspaceFolder::isIgnoredFile(const std::filesystem::path& path, const std::optional<ClientConfiguration>& givenConfig)
{
    // We want to test globs against a relative path to workspace, since thats what makes most sense
    auto relativePath = path.lexically_relative(rootUri.fsPath()).generic_string(); // HACK: we convert to generic string so we get '/' separators

    auto config = givenConfig ? *givenConfig : client->getConfiguration(rootUri);
    std::vector<std::string> patterns = config.ignoreGlobs; // TODO: extend further?
    for (auto& pattern : patterns)
    {
        if (glob::fnmatch_case(relativePath, pattern))
        {
            return true;
        }
    }
    return false;
}

lsp::DocumentDiagnosticReport WorkspaceFolder::documentDiagnostics(const lsp::DocumentDiagnosticParams& params)
{
    // TODO: should we apply a resultId and return an unchanged report if unchanged?
    lsp::DocumentDiagnosticReport report;
    std::unordered_map<std::string /* lsp::DocumentUri */, std::vector<lsp::Diagnostic>> relatedDiagnostics;

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    Luau::CheckResult cr;
    if (frontend.isDirty(moduleName))
        cr = frontend.check(moduleName);

    // If there was an error retrieving the source module, bail early with this diagnostic
    if (!frontend.getSourceModule(moduleName))
    {
        lsp::Diagnostic errorDiagnostic;
        errorDiagnostic.source = "Luau";
        errorDiagnostic.code = "000";
        errorDiagnostic.message = "Failed to resolve source module for this file";
        errorDiagnostic.severity = lsp::DiagnosticSeverity::Error;
        errorDiagnostic.range = {{0, 0}, {0, 0}};
        report.items.emplace_back(errorDiagnostic);
        return report;
    }

    auto config = client->getConfiguration(rootUri);

    // Report Type Errors
    // Note that type errors can extend to related modules in the require graph - so we report related information here
    for (auto& error : cr.errors)
    {
        auto diagnostic = createTypeErrorDiagnostic(error);
        if (error.moduleName == moduleName)
        {
            report.items.emplace_back(diagnostic);
        }
        else
        {
            auto fileName = fileResolver.resolveVirtualPathToRealPath(error.moduleName);
            if (!fileName || isIgnoredFile(*fileName, config))
                continue;
            auto uri = Uri::file(*fileName);
            auto& currentDiagnostics = relatedDiagnostics[uri.toString()];
            currentDiagnostics.emplace_back(diagnostic);
        }
    }

    // Convert the related diagnostics map into an equivalent report
    if (!relatedDiagnostics.empty())
    {
        for (auto& [uri, diagnostics] : relatedDiagnostics)
        {
            // TODO: resultId?
            lsp::SingleDocumentDiagnosticReport subReport{lsp::DocumentDiagnosticReportKind::Full, std::nullopt, diagnostics};
            report.relatedDocuments.emplace(uri, subReport);
        }
    }

    // Report Lint Warnings
    // Lints only apply to the current file
    Luau::LintResult lr = frontend.lint(moduleName);
    for (auto& error : lr.errors)
    {
        auto diagnostic = createLintDiagnostic(error);
        diagnostic.severity = lsp::DiagnosticSeverity::Error; // Report this as an error instead
        report.items.emplace_back(diagnostic);
    }
    for (auto& error : lr.warnings)
        report.items.emplace_back(createLintDiagnostic(error));

    return report;
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

        // Handle parentheses suggestions
        if (entry.parens == Luau::ParenthesesRecommendation::CursorAfter)
        {
            item.insertText = name + "()$0";
            item.insertTextFormat = lsp::InsertTextFormat::Snippet;
        }
        else if (entry.parens == Luau::ParenthesesRecommendation::CursorInside)
        {
            item.insertText = name + "($1)$0";
            item.insertTextFormat = lsp::InsertTextFormat::Snippet;
            // Trigger Signature Help
            item.command = lsp::Command{"Trigger Signature Help", "editor.action.triggerParameterHints"};
        }

        if (entry.type.has_value())
        {
            auto id = Luau::follow(entry.type.value());
            // Try to infer more type info about the entry to provide better suggestion info
            if (Luau::get<Luau::FunctionTypeVar>(id))
            {
                item.kind = lsp::CompletionItemKind::Function;
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

std::vector<lsp::DocumentLink> WorkspaceFolder::documentLink(const lsp::DocumentLinkParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    std::vector<lsp::DocumentLink> result;

    // We need to parse the code, which is currently only done in the type checker
    frontend.check(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule || !sourceModule->root)
        return {};

    // Only resolve document links on require(Foo.Bar.Baz) code
    // TODO: Curerntly we only link at the top level block, not nested blocks
    for (auto stat : sourceModule->root->body)
    {
        if (auto local = stat->as<Luau::AstStatLocal>())
        {
            if (local->values.size == 0)
                continue;

            for (size_t i = 0; i < local->values.size; i++)
            {
                const Luau::AstExprCall* call = local->values.data[i]->as<Luau::AstExprCall>();
                if (!call)
                    continue;

                if (auto maybeRequire = types::matchRequire(*call))
                {
                    if (auto moduleInfo = frontend.moduleResolver.resolveModuleInfo(moduleName, **maybeRequire))
                    {
                        // Resolve the module info to a URI
                        std::optional<std::filesystem::path> realName = moduleInfo->name;
                        if (fileResolver.isVirtualPath(moduleInfo->name))
                            realName = fileResolver.resolveVirtualPathToRealPath(moduleInfo->name);

                        if (realName)
                        {
                            lsp::DocumentLink link;
                            link.target = Uri::file(*realName);
                            link.range = lsp::Range{{call->argLocation.begin.line, call->argLocation.begin.column},
                                {call->argLocation.end.line, call->argLocation.end.column - 1}};
                            result.push_back(link);
                        }
                    }
                }
            }
        }
    }

    return result;
}

std::optional<lsp::Hover> WorkspaceFolder::hover(const lsp::HoverParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto position = convertPosition(params.position);

    // Run the type checker to ensure we are up to date
    // TODO: expressiveTypes - remove "forAutocomplete" once the types have been fixed
    Luau::FrontendOptions frontendOpts{true, true};
    frontend.check(moduleName, frontendOpts);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return std::nullopt;

    // TODO: fix forAutocomplete
    auto module = frontend.moduleResolverForAutocomplete.getModule(moduleName);
    auto exprOrLocal = Luau::findExprOrLocalAtPosition(*sourceModule, position);
    auto node = findNodeOrTypeAtPosition(*sourceModule, position);
    if (!node)
        return std::nullopt;

    std::string typeName;
    std::optional<Luau::TypeId> type = std::nullopt;

    if (auto ref = node->as<Luau::AstTypeReference>())
    {
        auto scope = Luau::findScopeAtPosition(*module, position);
        if (!scope)
            return std::nullopt;
        std::optional<Luau::TypeFun> typeFun;
        if (ref->prefix)
        {
            typeName = std::string(ref->prefix->value) + "." + ref->name.value;
            typeFun = scope->lookupImportedType(ref->prefix->value, ref->name.value);
        }
        else
        {
            typeName = ref->name.value;
            typeFun = scope->lookupType(ref->name.value);
        }
        if (!typeFun)
            return std::nullopt;
        type = typeFun->type;
    }
    else if (auto local = exprOrLocal.getLocal()) // TODO: can we just use node here instead of also calling exprOrLocal?
    {
        auto scope = Luau::findScopeAtPosition(*module, position);
        if (!scope)
            return std::nullopt;
        type = scope->lookup(local);
    }
    else if (auto expr = exprOrLocal.getExpr())
    {
        // Special case, we want to check if there is a parent in the ancestry, and if it is an AstTable
        // If so, and we are hovering over a prop, we want to give type info for the assigned expression to the prop
        // rather than just "string"
        auto ancestry = Luau::findAstAncestryOfPosition(*sourceModule, position);
        if (ancestry.size() >= 2 && ancestry.at(ancestry.size() - 2)->is<Luau::AstExprTable>())
        {
            auto parent = ancestry.at(ancestry.size() - 2)->as<Luau::AstExprTable>();
            for (const auto& [kind, key, value] : parent->items)
            {
                if (key && key->location.contains(position))
                {
                    // Return type type of the value
                    if (auto it = module->astTypes.find(value))
                    {
                        type = *it;
                    }
                    break;
                }
            }
        }

        if (!type)
        {
            if (auto it = module->astTypes.find(expr))
            {
                type = *it;
            }
            else if (auto index = expr->as<Luau::AstExprIndexName>())
            {
                if (auto parentIt = module->astTypes.find(index->expr))
                {
                    auto parentType = Luau::follow(*parentIt);
                    auto indexName = index->index.value;
                    auto prop = lookupProp(parentType, indexName);
                    if (prop)
                        type = prop->type;
                }
            }
        }
    }

    if (!type)
        return std::nullopt;
    type = Luau::follow(*type);

    Luau::ToStringOptions opts;
    opts.exhaustive = true;
    opts.useLineBreaks = true;
    opts.functionTypeArguments = true;
    opts.hideNamedFunctionTypeParameters = false;
    opts.indent = true;
    std::string typeString = Luau::toString(*type, opts);

    // If we have a function and its corresponding name
    if (!typeName.empty())
    {
        typeString = codeBlock("lua", "type " + typeName + " = " + typeString);
    }
    else if (auto ftv = Luau::get<Luau::FunctionTypeVar>(*type))
    {
        types::NameOrExpr name = exprOrLocal.getExpr();
        if (auto localName = exprOrLocal.getName())
        {
            name = localName->value;
        }
        typeString = codeBlock("lua", types::toStringNamedFunction(module, ftv, name));
    }
    else if (exprOrLocal.getLocal() || exprOrLocal.getExpr()->as<Luau::AstExprLocal>())
    {
        std::string builder = "local ";
        builder += exprOrLocal.getName()->value;
        builder += ": " + typeString;
        typeString = codeBlock("lua", builder);
    }
    else if (auto global = exprOrLocal.getExpr()->as<Luau::AstExprGlobal>())
    {
        // TODO: should we indicate this is a global somehow?
        std::string builder = "type ";
        builder += global->name.value;
        builder += " = " + typeString;
        typeString = codeBlock("lua", builder);
    }
    else
    {
        typeString = codeBlock("lua", typeString);
    }

    if (auto symbol = type.value()->documentationSymbol)
    {
        typeString += "\n----------\n";
        typeString += printDocumentation(client->documentation, *symbol);
    }

    return lsp::Hover{{lsp::MarkupKind::Markdown, typeString}};
}

std::optional<lsp::SignatureHelp> WorkspaceFolder::signatureHelp(const lsp::SignatureHelpParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto position = convertPosition(params.position);

    // Run the type checker to ensure we are up to date
    // TODO: expressiveTypes - remove "forAutocomplete" once the types have been fixed
    Luau::FrontendOptions frontendOpts{true, true};
    frontend.check(moduleName, frontendOpts);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return std::nullopt;

    auto module = frontend.moduleResolverForAutocomplete.getModule(moduleName);
    auto ancestry = Luau::findAstAncestryOfPosition(*sourceModule, position);

    if (ancestry.size() == 0)
        return std::nullopt;

    Luau::AstExprCall* candidate = ancestry.back()->as<Luau::AstExprCall>();
    if (!candidate && ancestry.size() >= 2)
        candidate = ancestry.at(ancestry.size() - 2)->as<Luau::AstExprCall>();

    if (!candidate)
        return std::nullopt;

    size_t activeParameter = candidate->args.size == 0 ? 0 : candidate->args.size - 1;

    auto it = module->astTypes.find(candidate->func);
    if (!it)
        return std::nullopt;
    auto followedId = Luau::follow(*it);

    std::vector<lsp::SignatureInformation> signatures;

    auto addSignature = [&](const Luau::FunctionTypeVar* ftv)
    {
        Luau::ToStringOptions opts;
        opts.functionTypeArguments = true;
        opts.hideNamedFunctionTypeParameters = false;
        opts.hideFunctionSelfArgument = candidate->self; // If self has been provided, then hide the self argument

        // Create the whole label
        std::string label = types::toStringNamedFunction(module, ftv, candidate->func);
        lsp::MarkupContent documentation{lsp::MarkupKind::PlainText, ""};

        if (followedId->documentationSymbol)
            documentation = {lsp::MarkupKind::Markdown, printDocumentation(client->documentation, *followedId->documentationSymbol)};

        // Create each parameter label
        std::vector<lsp::ParameterInformation> parameters;
        auto it = Luau::begin(ftv->argTypes);
        size_t idx = 0;

        while (it != Luau::end(ftv->argTypes))
        {
            // If the function has self, and the caller has called as a method (i.e., :), then omit the self parameter
            if (idx == 0 && ftv->hasSelf && candidate->self)
            {
                it++;
                idx++;
                continue;
            }

            std::string label;
            lsp::MarkupContent parameterDocumentation{lsp::MarkupKind::PlainText, ""};
            if (idx < ftv->argNames.size() && ftv->argNames[idx])
            {
                label = ftv->argNames[idx]->name + ": ";
            }
            label += Luau::toString(*it);

            parameters.push_back(lsp::ParameterInformation{label, parameterDocumentation});
            it++;
            idx++;
        }

        signatures.push_back(lsp::SignatureInformation{label, documentation, parameters});
    };

    if (auto ftv = Luau::get<Luau::FunctionTypeVar>(followedId))
    {
        // Single function
        addSignature(ftv);
    }

    // Handle overloaded function
    if (auto intersect = Luau::get<Luau::IntersectionTypeVar>(followedId))
    {
        for (Luau::TypeId part : intersect->parts)
        {
            if (auto candidateFunctionType = Luau::get<Luau::FunctionTypeVar>(part))
            {
                addSignature(candidateFunctionType);
            }
        }
    }

    return lsp::SignatureHelp{signatures, 0, activeParameter};
}

std::optional<lsp::Location> WorkspaceFolder::gotoDefinition(const lsp::DefinitionParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto position = convertPosition(params.position);

    // Run the type checker to ensure we are up to date
    if (frontend.isDirty(moduleName))
    {
        // TODO: expressiveTypes - remove "forAutocomplete" once the types have been fixed
        Luau::FrontendOptions frontendOpts{true, true};
        frontend.check(moduleName, frontendOpts);
    }

    auto sourceModule = frontend.getSourceModule(moduleName);
    // TODO: fix "forAutocomplete"
    auto module = frontend.moduleResolverForAutocomplete.getModule(moduleName);
    if (!sourceModule || !module)
        return std::nullopt;

    auto binding = Luau::findBindingAtPosition(*module, *sourceModule, position);
    if (binding)
        // TODO: can we maybe get further references if it points to something like `local X = require(...)`?
        return lsp::Location{params.textDocument.uri, lsp::Range{convertPosition(binding->location.begin), convertPosition(binding->location.end)}};

    auto node = findNodeOrTypeAtPosition(*sourceModule, position);
    if (!node)
        return std::nullopt;

    if (auto expr = node->asExpr())
    {
        if (auto lvalue = Luau::tryGetLValue(*expr))
        {
            const Luau::LValue* current = &*lvalue;
            std::vector<std::string> keys; // keys in reverse order
            while (auto field = Luau::get<Luau::Field>(*current))
            {
                keys.push_back(field->key);
                current = baseof(*current);
            }

            const Luau::Symbol* symbol = Luau::get<Luau::Symbol>(*current);
            auto scope = Luau::findScopeAtPosition(*module, position);
            if (!scope)
                return std::nullopt;

            auto baseType = scope->lookup(*symbol);
            if (!baseType)
                return std::nullopt;
            baseType = Luau::follow(*baseType);

            std::vector<Luau::Property> properties;
            for (auto it = keys.rbegin(); it != keys.rend(); ++it)
            {
                auto base = properties.empty() ? *baseType : Luau::follow(properties.back().type);
                auto prop = lookupProp(base, *it);
                if (!prop)
                    return std::nullopt;
                properties.push_back(*prop);
            }

            if (properties.empty())
                return std::nullopt;

            std::optional<Luau::ModuleName> definitionModuleName = std::nullopt;
            std::optional<Luau::Location> location = std::nullopt;

            for (auto it = properties.rbegin(); it != properties.rend(); ++it)
            {
                if (!location && it->location)
                    location = it->location;
                if (!definitionModuleName)
                    definitionModuleName = Luau::getDefinitionModuleName(Luau::follow(it->type));
            }

            if (location)
            {
                if (definitionModuleName)
                {
                    if (auto file = fileResolver.resolveVirtualPathToRealPath(*definitionModuleName))
                    {
                        return lsp::Location{Uri::file(*file), lsp::Range{convertPosition(location->begin), convertPosition(location->end)}};
                    }
                }
                return lsp::Location{params.textDocument.uri, lsp::Range{convertPosition(location->begin), convertPosition(location->end)}};
            }

            // auto ty = Luau::follow(prop->type);

            // Try to see if we can infer a good location
            // if (auto ftv = Luau::get<Luau::FunctionTypeVar>(ty))
            // {
            //     if (auto def = ftv->definition)
            //     {
            //         if (auto definitionModuleName = def->definitionModuleName)
            //         {
            //             auto file = fileResolver.resolveVirtualPathToRealPath(*definitionModuleName);
            //             if (file)
            //                 return lsp::Location{Uri::file(*file),
            //                     lsp::Range{convertPosition(def->definitionLocation.begin), convertPosition(def->definitionLocation.end)}};
            //         }
            //         else
            //         {
            //             return lsp::Location{params.textDocument.uri,
            //                 lsp::Range{convertPosition(def->definitionLocation.begin), convertPosition(def->definitionLocation.end)}};
            //         }
            //     }
            // }

            // Fallback to the prop location if available
            // if (prop->location)
            // {
            //     if (auto definitionModuleName = Luau::getDefinitionModuleName(ty))
            //     {
            //         if (auto file = fileResolver.resolveVirtualPathToRealPath(*definitionModuleName))
            //         {
            //             return lsp::Location{
            //                 Uri::file(*file), lsp::Range{convertPosition(prop->location->begin), convertPosition(prop->location->end)}};
            //         }
            //     }
            //     return lsp::Location{
            //         params.textDocument.uri, lsp::Range{convertPosition(prop->location->begin), convertPosition(prop->location->end)}};
            // }
        }
    }

    return std::nullopt;
}

std::optional<lsp::Location> WorkspaceFolder::gotoTypeDefinition(const lsp::TypeDefinitionParams& params)
{
    // If its a binding, we should find its assigned type if possible, and then find the definition of that type
    // If its a type, then just find the definintion of that type (i.e. the type alias)

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto position = convertPosition(params.position);

    // Run the type checker to ensure we are up to date
    if (frontend.isDirty(moduleName))
    {
        // TODO: expressiveTypes - remove "forAutocomplete" once the types have been fixed
        Luau::FrontendOptions frontendOpts{true, true};
        frontend.check(moduleName, frontendOpts);
    }


    auto sourceModule = frontend.getSourceModule(moduleName);
    // TODO: fix "forAutocomplete"
    auto module = frontend.moduleResolverForAutocomplete.getModule(moduleName);
    if (!sourceModule || !module)
        return std::nullopt;

    auto node = findNodeOrTypeAtPosition(*sourceModule, position);
    if (!node)
        return std::nullopt;

    auto findTypeLocation = [&module, &position, &params](Luau::AstType* type) -> std::optional<lsp::Location>
    {
        // TODO: should we only handle references here? what if its an actual type
        if (auto reference = type->as<Luau::AstTypeReference>())
        {
            auto scope = Luau::findScopeAtPosition(*module, position);
            if (!scope)
                return std::nullopt;

            // TODO: we currently can't handle if its imported from a module
            if (reference->prefix)
                return std::nullopt;

            auto location = lookupTypeLocation(*scope, reference->name.value);
            if (!location)
                return std::nullopt;

            return lsp::Location{params.textDocument.uri, lsp::Range{convertPosition(location->begin), convertPosition(location->end)}};
        }
        return std::nullopt;
    };

    if (auto type = node->asType())
    {
        return findTypeLocation(type);
    }
    else if (auto typeAlias = node->as<Luau::AstStatTypeAlias>())
    {
        return findTypeLocation(typeAlias->type);
    }
    else if (auto localExpr = node->as<Luau::AstExprLocal>())
    {
        if (auto local = localExpr->local)
        {
            if (auto annotation = local->annotation)
            {
                return findTypeLocation(annotation);
            }
        }
    }

    return std::nullopt;
}

lsp::ReferenceResult WorkspaceFolder::references(const lsp::ReferenceParams& params)
{
    // TODO: currently we only support searching for a binding at a current position
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto position = convertPosition(params.position);

    // Run the type checker to ensure we are up to date
    // TODO: we only need the parse result here - can typechecking be skipped?
    if (frontend.isDirty(moduleName))
        frontend.check(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return std::nullopt;

    auto exprOrLocal = Luau::findExprOrLocalAtPosition(*sourceModule, position);
    Luau::Symbol symbol;

    if (exprOrLocal.getLocal())
        symbol = exprOrLocal.getLocal();
    else if (auto exprLocal = exprOrLocal.getExpr()->as<Luau::AstExprLocal>())
        symbol = exprLocal->local;
    else
        return std::nullopt;

    auto references = findSymbolReferences(*sourceModule, symbol);
    std::vector<lsp::Location> result;

    for (auto& location : references)
    {
        result.emplace_back(lsp::Location{params.textDocument.uri, {convertPosition(location.begin), convertPosition(location.end)}});
    }

    return result;
}

lsp::RenameResult WorkspaceFolder::rename(const lsp::RenameParams& params)
{
    // Verify the new name is valid (is an identifier)
    if (params.newName.length() == 0)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "The new name must be a valid identifier");
    if (!isalpha(params.newName.at(0)) && params.newName.at(0) != '_')
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "The new name must be a valid identifier starting with a character or underscore");
    for (auto ch : params.newName)
    {
        if (!isalpha(ch) && !isdigit(ch) && ch != '_')
            throw JsonRpcException(
                lsp::ErrorCode::RequestFailed, "The new name must be a valid identifier composed of characters, digits, and underscores only");
    }

    // TODO: currently we only support renaming local bindings in the current file
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto position = convertPosition(params.position);

    // Run the type checker to ensure we are up to date
    // TODO: we only need the parse result here - can typechecking be skipped?
    if (frontend.isDirty(moduleName))
        frontend.check(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "Unable to read source code");

    auto exprOrLocal = Luau::findExprOrLocalAtPosition(*sourceModule, position);
    Luau::Symbol symbol;

    if (exprOrLocal.getLocal())
        symbol = exprOrLocal.getLocal();
    else if (auto exprLocal = exprOrLocal.getExpr()->as<Luau::AstExprLocal>())
        symbol = exprLocal->local;
    else
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "Rename is currently only supported for local variable bindings in the current file");

    auto references = findSymbolReferences(*sourceModule, symbol);
    std::vector<lsp::TextEdit> localChanges;
    for (auto& location : references)
    {
        localChanges.emplace_back(lsp::TextEdit{{convertPosition(location.begin), convertPosition(location.end)}, params.newName});
    }

    return lsp::WorkspaceEdit{{{params.textDocument.uri.toString(), localChanges}}};
}

bool WorkspaceFolder::updateSourceMap()
{
    // Read in the sourcemap
    // TODO: we assume a sourcemap.json file in the workspace root
    if (auto sourceMapContents = readFile(rootUri.fsPath() / "sourcemap.json"))
    {
        frontend.clear();
        fileResolver.updateSourceMap(sourceMapContents.value());

        types::registerInstanceTypes(frontend.typeChecker, fileResolver, /* TODO - expressiveTypes: */ false);
        types::registerInstanceTypes(frontend.typeCheckerForAutocomplete, fileResolver);

        // TODO: we should signal a diagnostics refresh

        return true;
    }
    else
    {
        return false;
    }
}

void WorkspaceFolder::setup()
{
    Luau::registerBuiltinTypes(frontend.typeChecker);
    Luau::registerBuiltinTypes(frontend.typeCheckerForAutocomplete);

    if (client->definitionsFile)
    {
        client->sendLogMessage(lsp::MessageType::Info, "Loading definitions file: " + client->definitionsFile->generic_string());
        auto result = types::registerDefinitions(frontend.typeChecker, *client->definitionsFile);
        types::registerDefinitions(frontend.typeCheckerForAutocomplete, *client->definitionsFile);

        if (!result.success)
        {
            client->sendWindowMessage(lsp::MessageType::Error, "Failed to read definitions file. Extended types will not be provided");
            // TODO: Display diagnostics?
        }
    }
    else
    {
        client->sendLogMessage(lsp::MessageType::Error, "Definitions file was not provided by the client. Extended types will not be provided");
        client->sendWindowMessage(lsp::MessageType::Error, "Definitions file was not provided by the client. Extended types will not be provided");
    }
    Luau::freeze(frontend.typeChecker.globalTypes);
    Luau::freeze(frontend.typeCheckerForAutocomplete.globalTypes);

    if (!isNullWorkspace() && !updateSourceMap())
    {
        client->sendWindowMessage(
            lsp::MessageType::Error, "Failed to load sourcemap.json for workspace '" + name + "'. Instance information will not be available");
    }
}