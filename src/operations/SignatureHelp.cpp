#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"

std::optional<lsp::SignatureHelp> WorkspaceFolder::signatureHelp(const lsp::SignatureHelpParams& params)
{
    auto config = client->getConfiguration(rootUri);

    if (!config.signatureHelp.enabled)
        return std::nullopt;

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(moduleName);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + moduleName);
    auto position = textDocument->convertPosition(params.position);

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

    types::ToStringNamedFunctionOpts opts;
    opts.hideTableKind = !config.hover.showTableKinds;

    std::vector<lsp::SignatureInformation> signatures;

    auto addSignature = [&](const Luau::FunctionTypeVar* ftv)
    {
        // Create the whole label
        std::string label = types::toStringNamedFunction(module, ftv, candidate->func, scope, opts);
        lsp::MarkupContent documentation{lsp::MarkupKind::PlainText, ""};

        if (followedId->documentationSymbol)
            documentation = {lsp::MarkupKind::Markdown, printDocumentation(client->documentation, *followedId->documentationSymbol)};
        else if (ftv->definition && ftv->definition->definitionModuleName)
            documentation = {lsp::MarkupKind::Markdown,
                printMoonwaveDocumentation(getComments(ftv->definition->definitionModuleName.value(), ftv->definition->definitionLocation))};

        // Create each parameter label
        std::vector<lsp::ParameterInformation> parameters;
        auto it = Luau::begin(ftv->argTypes);
        size_t idx = 0;

        while (it != Luau::end(ftv->argTypes))
        {
            // If the function has self, and the caller has called as a method (i.e., :), then omit the self parameter
            // TODO: hasSelf is not always specified, so we manually check for the "self" name (https://github.com/Roblox/luau/issues/551)
            if (idx == 0 && (ftv->hasSelf || (ftv->argNames.size() > 0 && ftv->argNames[0].has_value() && ftv->argNames[0]->name == "self")) &&
                candidate->self)
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

std::optional<lsp::SignatureHelp> LanguageServer::signatureHelp(const lsp::SignatureHelpParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->signatureHelp(params);
}