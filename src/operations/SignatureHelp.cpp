#include "LSP/Workspace.hpp"

#include "Luau/AstQuery.h"
#include "Luau/Normalize.h"
#include "Luau/Unifier.h"
#include "Luau/OverloadResolution.h"
#include "LSP/LuauExt.hpp"
#include "LSP/DocumentationParser.hpp"

LUAU_FASTINT(LuauTypeInferRecursionLimit)
LUAU_FASTINT(LuauTypeInferIterationLimit)

// Taken from Luau/Autocomplete.cpp
static bool checkOverloadMatch(Luau::TypePackId subTp, Luau::TypePackId superTp, Luau::NotNull<Luau::Scope> scope, Luau::TypeArena* typeArena,
    Luau::NotNull<Luau::BuiltinTypes> builtinTypes)
{
    Luau::InternalErrorReporter iceReporter;
    Luau::UnifierSharedState unifierState(&iceReporter);
    Luau::SimplifierPtr simplifier = newSimplifier(Luau::NotNull{typeArena}, builtinTypes);
    Luau::Normalizer normalizer{
        typeArena, builtinTypes, Luau::NotNull{&unifierState}, FFlag::LuauSolverV2 ? Luau::SolverMode::New : Luau::SolverMode::Old};

    if (FFlag::LuauSolverV2)
    {
        Luau::TypeCheckLimits limits;
        Luau::TypeFunctionRuntime typeFunctionRuntime{
            Luau::NotNull{&iceReporter}, Luau::NotNull{&limits}
        }; // TODO: maybe subtyping checks should not invoke user-defined type function runtime

        unifierState.counters.recursionLimit = FInt::LuauTypeInferRecursionLimit;
        unifierState.counters.iterationLimit = FInt::LuauTypeInferIterationLimit;

        Luau::Subtyping subtyping{builtinTypes, Luau::NotNull{typeArena}, Luau::NotNull{simplifier.get()}, Luau::NotNull{&normalizer}, Luau::NotNull{&typeFunctionRuntime}, Luau::NotNull{&iceReporter}};

        // DEVIATION: the flip for superTp and subTp is expected
        // subTp is our custom created type pack, with a trailing ...any
        // so it is actually more general than superTp - we want to check if superTp can match against it.
        return subtyping.isSubtype(superTp, subTp, scope, {}).isSubtype;
    }
    else
    {
        Luau::Unifier unifier(Luau::NotNull<Luau::Normalizer>{&normalizer}, scope, Luau::Location(), Luau::Variance::Covariant);

        // Cost of normalization can be too high for autocomplete response time requirements
        unifier.normalize = false;
        unifier.checkInhabited = false;

        return unifier.canUnify(subTp, superTp).empty();
    }
}

std::optional<lsp::SignatureHelp> WorkspaceFolder::signatureHelp(
    const lsp::SignatureHelpParams& params, const LSPCancellationToken& cancellationToken)
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
    checkStrict(moduleName, cancellationToken);
    throwIfCancelled(cancellationToken);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return std::nullopt;

    auto module = getModule(moduleName, /* forAutocomplete: */ true);
    auto ancestry = Luau::findAstAncestryOfPosition(*sourceModule, position);
    auto scope = Luau::findScopeAtPosition(*module, position);

    if (ancestry.size() == 0 || !scope)
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

    // Construct a type pack from the current list of arguments for overload matching
    Luau::TypeArena typeArena;
    std::vector<Luau::TypeId> argumentTys;
    if (candidate->self)
        argumentTys.push_back(followedId);
    for (auto&& arg : candidate->args)
        if (auto ty = module->astTypes.find(arg))
            argumentTys.push_back(Luau::follow(*ty));
    Luau::TypePackId subTp = typeArena.addTypePack(argumentTys, frontend.builtinTypes->anyTypePack);

    types::ToStringNamedFunctionOpts opts;
    opts.hideTableKind = !config.hover.showTableKinds;

    std::optional<size_t> activeSignature = std::nullopt;
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
        size_t previousParamPos = label.find('('); // start search at start of parameter list, not earlier

        for (; it != Luau::end(ftv->argTypes); it++, idx++)
        {
            // If the function has self, and the caller has called as a method (i.e., :), then omit the self parameter
            if (idx == 0 && isMethod(ftv) && candidate->self)
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

        // If this overload matches, and we haven't yet found a match, mark it as the active signature
        if (!activeSignature && checkOverloadMatch(subTp, ftv->argTypes, Luau::NotNull{&*scope}, &typeArena, frontend.builtinTypes))
            activeSignature = signatures.size();

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

    // Handle __call metamethod
    if (const auto metamethod = findCallMetamethod(followedId))
        if (auto ftv = Luau::get<Luau::FunctionType>(Luau::follow(*metamethod)))
            addSignature(*metamethod, ftv);

    lsp::SignatureHelp help = lsp::SignatureHelp{signatures, activeSignature.value_or(0), activeParameter};
    platform->handleSignatureHelp(*textDocument, *sourceModule, position, help);

    return help;
}
