#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"

#include "Luau/AstQuery.h"
#include "LSP/LuauExt.hpp"
#include "LSP/DocumentationParser.hpp"

std::optional<lsp::SignatureHelp> WorkspaceFolder::signatureHelp(const lsp::SignatureHelpParams& params)
{
    auto config = client->getConfiguration(rootUri);

    if (!config.signatureHelp.enabled)
        return std::nullopt;

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());
    auto position = textDocument->convertPosition(params.position);

    // Run the type checker to ensure we are up to date
    // TODO: expressiveTypes - remove "forAutocomplete" once the types have been fixed
    checkStrict(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return std::nullopt;

    auto module = frontend.moduleResolverForAutocomplete.getModule(moduleName);
    auto ancestry = Luau::findAstAncestryOfPosition(*sourceModule, position);
    auto scope = Luau::findScopeAtPosition(*module, position);

    if (ancestry.size() == 0)
        return std::nullopt;

    auto* candidate = ancestry.back()->as<Luau::AstExprCall>();
    if (!candidate && ancestry.size() >= 2)
        candidate = ancestry.at(ancestry.size() - 2)->as<Luau::AstExprCall>();

    if (!candidate)
        return std::nullopt;

    // FIXME: should not be necessary if the `ty` has the doc symbol attached to it
    auto documentationSymbol = Luau::getDocumentationSymbolAtPosition(
        *sourceModule, *module, {candidate->func->location.end.line, candidate->func->location.end.column - 1});
    size_t activeParameter = 0;

    // Use the position to determine which parameter is active
    for (auto param : candidate->args)
    {
        if (param->location.containsClosed(position) || param->location.begin > position)
            break;
        activeParameter++;
    }

    auto it = module->astTypes.find(candidate->func);
    if (!it)
        return std::nullopt;
    auto followedId = Luau::follow(*it);

    types::ToStringNamedFunctionOpts opts;
    opts.hideTableKind = !config.hover.showTableKinds;

    std::vector<lsp::SignatureInformation> signatures{};

    auto addSignature = [&](const Luau::TypeId& ty, const Luau::FunctionType* ftv, bool isOverloaded = false)
    {
        // Create the whole label
        std::string label = types::toStringNamedFunction(module, ftv, candidate->func, scope, opts);
        lsp::MarkupContent documentation{lsp::MarkupKind::Markdown, ""};

        auto baseDocumentationSymbol = documentationSymbol;
        if (baseDocumentationSymbol && isOverloaded)
        {
            // We need to trim "/overload/" from the base symbol if its been resolved to something
            // FIXME: can be removed once we use docSymbol from `ty`
            if (auto idx = baseDocumentationSymbol->find("/overload/"); idx != std::string::npos)
                baseDocumentationSymbol = baseDocumentationSymbol->substr(0, idx);
            baseDocumentationSymbol = *baseDocumentationSymbol + "/overload/" + toString(ty);
        }

        if (std::optional<std::string> docs;
            baseDocumentationSymbol && (docs = printDocumentation(client->documentation, *baseDocumentationSymbol)) && docs)
            documentation.value = *docs;
        else if (ftv->definition && ftv->definition->definitionModuleName)
            documentation.value =
                printMoonwaveDocumentation(getComments(ftv->definition->definitionModuleName.value(), ftv->definition->definitionLocation));

        // Create each parameter label
        std::vector<lsp::ParameterInformation> parameters{};
        auto it = Luau::begin(ftv->argTypes);
        size_t idx = 0;
        size_t previousParamPos = 0;

        for (; it != Luau::end(ftv->argTypes); it++, idx++)
        {
            // If the function has self, and the caller has called as a method (i.e., :), then omit the self parameter
            // TODO: hasSelf is not always specified, so we manually check for the "self" name (https://github.com/Roblox/luau/issues/551)
            if (idx == 0 && (ftv->hasSelf || (ftv->argNames.size() > 0 && ftv->argNames[0].has_value() && ftv->argNames[0]->name == "self")) &&
                candidate->self)
                continue;


            // Show parameter documentation
            // TODO: parse moonwave docs for param documentation?
            lsp::MarkupContent parameterDocumentation{lsp::MarkupKind::Markdown, ""};
            if (baseDocumentationSymbol)
                if (auto docs = printDocumentation(client->documentation, *baseDocumentationSymbol + "/param/" + std::to_string(idx)))
                    parameterDocumentation.value = *docs;

            // Compute the label
            // We attempt to search for the position in the string for this label, and if we don't find it,
            // then we give up and just use the string label
            std::variant<std::string, std::vector<size_t>> paramLabel;
            std::string labelString;
            if (idx < ftv->argNames.size() && ftv->argNames[idx] && ftv->argNames[idx]->name != "_")
                labelString = ftv->argNames[idx]->name + ": ";
            labelString += Luau::toString(*it);

            auto position = label.find(labelString, previousParamPos);
            if (position != std::string::npos)
            {
                auto length = labelString.size();
                previousParamPos = position + length;
                paramLabel = std::vector{position, position + length};
            }
            else
                paramLabel = labelString;

            parameters.push_back(lsp::ParameterInformation{paramLabel, parameterDocumentation});
        }

        // Handle varargs
        if (auto tp = it.tail())
        {
            if (auto vtp = Luau::get<Luau::VariadicTypePack>(*tp); !vtp || !vtp->hidden)
            {
                // Show parameter documentation
                // TODO: parse moonwave docs for param documentation?
                lsp::MarkupContent parameterDocumentation{lsp::MarkupKind::Markdown, ""};
                if (baseDocumentationSymbol)
                    if (auto docs = printDocumentation(client->documentation, *baseDocumentationSymbol + "/param/" + std::to_string(idx)))
                        parameterDocumentation.value = *docs;

                // Compute the label
                // We attempt to search for the position in the string for this label, and if we don't find it,
                // then we give up and just use the string label
                std::variant<std::string, std::vector<size_t>> paramLabel;
                std::string labelString = "...: ";

                if (vtp)
                    labelString += Luau::toString(vtp->ty);
                else
                    labelString += Luau::toString(*tp);

                auto position = label.find(labelString, previousParamPos);
                if (position != std::string::npos)
                {
                    auto length = labelString.size();
                    previousParamPos = position + length;
                    paramLabel = std::vector{position, position + length};
                }
                else
                    paramLabel = labelString;

                parameters.push_back(lsp::ParameterInformation{paramLabel, parameterDocumentation});
            }
        }

        signatures.push_back(lsp::SignatureInformation{
            label, documentation, parameters, std::min(activeParameter, parameters.size() == 0 ? 0 : parameters.size() - 1)});
    };

    // Handle single function
    if (auto ftv = Luau::get<Luau::FunctionType>(followedId))
        addSignature(followedId, ftv);

    // Handle overloaded function
    if (auto intersect = Luau::get<Luau::IntersectionType>(followedId))
        for (Luau::TypeId part : intersect->parts)
            if (auto candidateFunctionType = Luau::get<Luau::FunctionType>(part))
                addSignature(part, candidateFunctionType, /* isOverloaded = */ true);

    return lsp::SignatureHelp{signatures, 0, activeParameter};
}

std::optional<lsp::SignatureHelp> LanguageServer::signatureHelp(const lsp::SignatureHelpParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->signatureHelp(params);
}