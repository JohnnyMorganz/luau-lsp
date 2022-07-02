#include <iostream>
#include <limits.h>
#include "LSP/Workspace.hpp"

/// Checks whether a provided file is part of the workspace
bool WorkspaceFolder::isInWorkspace(const lsp::DocumentUri& file)
{
    if (file == rootUri)
        return true;

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

void WorkspaceFolder::updateTextDocument(
    const lsp::DocumentUri& uri, const lsp::DidChangeTextDocumentParams& params, std::vector<Luau::ModuleName>* markedDirty)
{
    auto moduleName = fileResolver.getModuleName(uri);

    if (!contains(fileResolver.managedFiles, moduleName))
    {
        // Check if we have the original file URI stored (https://github.com/JohnnyMorganz/luau-lsp/issues/26)
        // TODO: can be potentially removed when server generates sourcemap
        auto fsPath = uri.fsPath().generic_string();
        if (fsPath != moduleName && contains(fileResolver.managedFiles, fsPath))
        {
            // Change the managed file key to use the new modulename
            auto nh = fileResolver.managedFiles.extract(fsPath);
            nh.key() = moduleName;
            fileResolver.managedFiles.insert(std::move(nh));
        }
        else
        {
            client->sendLogMessage(lsp::MessageType::Error, "Text Document not loaded locally: " + uri.toString());
            return;
        }
    }
    auto& textDocument = fileResolver.managedFiles.at(moduleName);
    textDocument.update(params.contentChanges, params.textDocument.version);

    // Mark the module dirty for the typechecker
    frontend.markDirty(moduleName, markedDirty);
}

void WorkspaceFolder::closeTextDocument(const lsp::DocumentUri& uri)
{
    auto moduleName = fileResolver.getModuleName(uri);
    fileResolver.managedFiles.erase(moduleName);

    // Clear out base uri fsPath as well, in case we managed it like that
    // TODO: can be potentially removed when server generates sourcemap
    fileResolver.managedFiles.erase(uri.fsPath().generic_string());

    // Mark the module as dirty as we no longer track its changes
    frontend.markDirty(moduleName);
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

bool WorkspaceFolder::isDefinitionFile(const std::filesystem::path& path, const std::optional<ClientConfiguration>& givenConfig)
{
    auto config = givenConfig ? *givenConfig : client->getConfiguration(rootUri);
    auto canonicalised = std::filesystem::weakly_canonical(path);

    for (auto& file : config.types.definitionFiles)
    {
        if (std::filesystem::weakly_canonical(file) == canonicalised)
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
    Luau::CheckResult cr = frontend.check(moduleName);

    // If there was an error retrieving the source module
    // Bail early with an empty report - it is likely that the file was closed
    if (!frontend.getSourceModule(moduleName))
        return report;

    auto config = client->getConfiguration(rootUri);

    // If the file is a definitions file, then don't display any diagnostics
    if (isDefinitionFile(params.textDocument.uri.fsPath(), config))
        return report;

    // If the file is ignored, and is *not* loaded in, then don't display any diagnostics
    if (isIgnoredFile(params.textDocument.uri.fsPath()) && !fileResolver.isManagedFile(moduleName))
        return report;

    // Report Type Errors
    // Note that type errors can extend to related modules in the require graph - so we report related information here
    for (auto& error : cr.errors)
    {
        auto diagnostic = createTypeErrorDiagnostic(error, &fileResolver);
        if (error.moduleName == moduleName)
        {
            report.items.emplace_back(diagnostic);
        }
        else
        {
            auto fileName = fileResolver.resolveToRealPath(error.moduleName);
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

lsp::WorkspaceDiagnosticReport WorkspaceFolder::workspaceDiagnostics(const lsp::WorkspaceDiagnosticParams& params)
{
    lsp::WorkspaceDiagnosticReport workspaceReport;
    auto config = client->getConfiguration(rootUri);

    // If we don't have workspace diagnostics enabled, then just return an empty report
    if (!config.diagnostics.workspace)
        return workspaceReport;

    // TODO: we should handle non-sourcemap features
    std::vector<SourceNodePtr> queue;
    if (fileResolver.rootSourceNode)
    {
        queue.push_back(fileResolver.rootSourceNode);
    };

    while (!queue.empty())
    {
        auto node = queue.back();
        queue.pop_back();
        for (auto& child : node->children)
        {
            queue.push_back(child);
        }

        auto realPath = fileResolver.getRealPathFromSourceNode(node);
        auto moduleName = fileResolver.getVirtualPathFromSourceNode(node);

        if (!realPath || isIgnoredFile(*realPath, config))
            continue;

        // Compute new check result
        Luau::CheckResult cr = frontend.check(moduleName);

        // If there was an error retrieving the source module, disregard this file
        // TODO: should we file a diagnostic?
        if (!frontend.getSourceModule(moduleName))
            continue;

        lsp::WorkspaceDocumentDiagnosticReport documentReport;
        documentReport.uri = Uri::file(*realPath);
        documentReport.kind = lsp::DocumentDiagnosticReportKind::Full;
        if (fileResolver.isManagedFile(moduleName))
        {
            documentReport.version = fileResolver.managedFiles.at(moduleName).version();
        }

        // Report Type Errors
        // Only report errors for the current file
        for (auto& error : cr.errors)
        {
            auto diagnostic = createTypeErrorDiagnostic(error, &fileResolver);
            if (error.moduleName == moduleName)
            {
                documentReport.items.emplace_back(diagnostic);
            }
        }

        // Report Lint Warnings
        Luau::LintResult lr = frontend.lint(moduleName);
        for (auto& error : lr.errors)
        {
            auto diagnostic = createLintDiagnostic(error);
            diagnostic.severity = lsp::DiagnosticSeverity::Error; // Report this as an error instead
            documentReport.items.emplace_back(diagnostic);
        }
        for (auto& error : lr.warnings)
            documentReport.items.emplace_back(createLintDiagnostic(error));

        workspaceReport.items.emplace_back(documentReport);
    }

    return workspaceReport;
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
                        auto realName = fileResolver.resolveToRealPath(moduleInfo->name);
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
    auto scope = Luau::findScopeAtPosition(*module, position);
    if (!node || !scope)
        return std::nullopt;

    std::string typeName;
    std::optional<Luau::TypeId> type = std::nullopt;

    if (auto ref = node->as<Luau::AstTypeReference>())
    {
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
            else if (auto global = expr->as<Luau::AstExprGlobal>())
            {
                type = scope->lookup(global->name);
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
    opts.scope = scope;
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
        typeString = codeBlock("lua", types::toStringNamedFunction(module, ftv, name, scope));
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
    auto scope = Luau::findScopeAtPosition(*module, position);

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
        // Create the whole label
        std::string label = types::toStringNamedFunction(module, ftv, candidate->func, scope);
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
                    if (auto file = fileResolver.resolveToRealPath(*definitionModuleName))
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
    auto sourcemapPath = rootUri.fsPath() / "sourcemap.json";
    client->sendTrace("Updating sourcemap contents from " + sourcemapPath.generic_string());

    // Read in the sourcemap
    // TODO: we assume a sourcemap.json file in the workspace root
    if (auto sourceMapContents = readFile(sourcemapPath))
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

void WorkspaceFolder::initialize()
{
    Luau::registerBuiltinTypes(frontend.typeChecker);
    Luau::registerBuiltinTypes(frontend.typeCheckerForAutocomplete);

    if (client->definitionsFiles.empty())
    {
        client->sendLogMessage(lsp::MessageType::Warning, "No definitions file provided by client");
    }

    for (auto definitionsFile : client->definitionsFiles)
    {
        client->sendLogMessage(lsp::MessageType::Info, "Loading definitions file: " + definitionsFile.generic_string());
        auto result = types::registerDefinitions(frontend.typeChecker, definitionsFile);
        types::registerDefinitions(frontend.typeCheckerForAutocomplete, definitionsFile);

        auto uri = Uri::file(definitionsFile);

        if (result.success)
        {
            // Clear any set diagnostics
            client->publishDiagnostics({uri, std::nullopt, {}});
        }
        else
        {
            client->sendWindowMessage(lsp::MessageType::Error,
                "Failed to read definitions file " + definitionsFile.generic_string() + ". Extended types will not be provided");

            // Display relevant diagnostics
            std::vector<lsp::Diagnostic> diagnostics;
            for (auto& error : result.parseResult.errors)
                diagnostics.emplace_back(createParseErrorDiagnostic(error));

            if (result.module)
                for (auto& error : result.module->errors)
                    diagnostics.emplace_back(createTypeErrorDiagnostic(error, &fileResolver));

            client->publishDiagnostics({uri, std::nullopt, diagnostics});
        }
    }
    Luau::freeze(frontend.typeChecker.globalTypes);
    Luau::freeze(frontend.typeCheckerForAutocomplete.globalTypes);
}

void WorkspaceFolder::setupWithConfiguration(const ClientConfiguration& configuration)
{
    if (configuration.sourcemap.enabled)
    {
        if (!isNullWorkspace() && !updateSourceMap())
        {
            client->sendWindowMessage(
                lsp::MessageType::Error, "Failed to load sourcemap.json for workspace '" + name + "'. Instance information will not be available");
        }
    }
}