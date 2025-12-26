#include "Platform/RobloxPlatform.hpp"

#include "LSP/Workspace.hpp"
#include "LSP/Utils.hpp"
#include "Platform/AutoImports.hpp"
#include "Luau/PrettyPrinter.h"
#include "Platform/InstanceRequireAutoImporter.hpp"

#include <set>

lsp::WorkspaceEdit RobloxPlatform::computeOrganiseServicesEdit(const lsp::DocumentUri& uri)
{
    auto moduleName = fileResolver->getModuleName(uri);
    auto textDocument = fileResolver->getTextDocument(uri);

    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + uri.toString());

    workspaceFolder->frontend.parse(moduleName);

    auto sourceModule = workspaceFolder->frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return {};

    // Find all `local X = game:GetService("Service")`
    Luau::LanguageServer::AutoImports::RobloxFindImportsVisitor visitor;
    visitor.visit(sourceModule->root);

    if (visitor.serviceLineMap.empty())
        return {};

    // Test to see that if all the services are already sorted -> if they are, then just leave alone
    // to prevent clogging the undo history stack
    Luau::Location previousServiceLocation{{0, 0}, {0, 0}};
    bool isSorted = true;
    for (const auto& [_, stat] : visitor.serviceLineMap)
    {
        if (stat->location.begin < previousServiceLocation.begin)
        {
            isSorted = false;
            break;
        }
        previousServiceLocation = stat->location;
    }
    if (isSorted)
        return {};

    std::vector<lsp::TextEdit> edits;
    // We firstly delete all the previous services, as they will be added later
    edits.reserve(visitor.serviceLineMap.size());
    for (const auto& [_, stat] : visitor.serviceLineMap)
        edits.emplace_back(lsp::TextEdit{{{stat->location.begin.line, 0}, {stat->location.begin.line + 1, 0}}, ""});

    // We find the first line to add these services to, and then add them in sorted order
    lsp::Range insertLocation{{visitor.firstServiceDefinitionLine.value(), 0}, {visitor.firstServiceDefinitionLine.value(), 0}};
    for (const auto& [serviceName, stat] : visitor.serviceLineMap)
    {
        // We need to rewrite the statement as we expected it
        auto importText = Luau::toString(stat) + "\n";
        edits.emplace_back(lsp::TextEdit{insertLocation, importText});
    }

    lsp::WorkspaceEdit workspaceEdit;
    workspaceEdit.changes.emplace(uri, edits);
    return workspaceEdit;
}

void RobloxPlatform::handleCodeAction(const lsp::CodeActionParams& params, std::vector<lsp::CodeAction>& items)
{
    if (params.context.wants(lsp::CodeActionKind::Source) || params.context.wants(lsp::CodeActionKind::SourceOrganizeImports))
    {
        lsp::CodeAction sortServicesAction;
        sortServicesAction.title = "Sort services";
        sortServicesAction.kind = lsp::CodeActionKind::SourceOrganizeImports;
        sortServicesAction.edit = computeOrganiseServicesEdit(params.textDocument.uri);
        items.emplace_back(sortServicesAction);
    }
}

void RobloxPlatform::handleUnknownSymbolFix(const UnknownSymbolFixContext& ctx, const Luau::UnknownSymbol& unknownSymbol,
    const std::optional<lsp::Diagnostic>& diagnostic, std::vector<lsp::CodeAction>& result)
{
    if (unknownSymbol.context != Luau::UnknownSymbol::Binding)
        return;

    ClientConfiguration config = workspaceFolder->fileResolver.client->getConfiguration(workspaceFolder->rootUri);

    // Find existing imports
    Luau::LanguageServer::AutoImports::RobloxFindImportsVisitor importsVisitor;
    importsVisitor.visit(ctx.sourceModule->root);

    auto hotCommentsLineNumber = Luau::LanguageServer::AutoImports::computeHotCommentsLineNumber(*ctx.sourceModule);

    // 1. Check if the unknown symbol matches a Roblox service name
    std::optional<RobloxDefinitionsFileMetadata> metadata = workspaceFolder->definitionsFileMetadata;
    auto services = metadata.has_value() ? metadata->SERVICES : std::vector<std::string>{};
    bool foundServiceMatch = contains(services, unknownSymbol.name) && !contains(importsVisitor.serviceLineMap, unknownSymbol.name);
    if (foundServiceMatch)
    {
        size_t lineNumber = importsVisitor.findBestLineForService(unknownSymbol.name, hotCommentsLineNumber);

        bool appendNewline = false;
        if (config.completion.imports.separateGroupsWithLine && importsVisitor.firstRequireLine &&
            importsVisitor.firstRequireLine.value() - lineNumber == 0)
            appendNewline = true;

        auto serviceEdit = Luau::LanguageServer::AutoImports::createServiceTextEdit(unknownSymbol.name, lineNumber, appendNewline);

        lsp::CodeAction action;
        action.title = "Import service '" + unknownSymbol.name + "'";
        action.kind = lsp::CodeActionKind::QuickFix;
        action.isPreferred = true;

        if (diagnostic)
            action.diagnostics.push_back(*diagnostic);

        lsp::WorkspaceEdit workspaceEdit;
        workspaceEdit.changes.emplace(ctx.uri, std::vector{serviceEdit});
        action.edit = workspaceEdit;

        result.push_back(action);
    }

    // 2. Check for modules that can be required
    if (config.completion.imports.stringRequires.enabled)
    {
        LSPPlatform::handleUnknownSymbolFix(ctx, unknownSymbol, diagnostic, result);
    }
    else
    {
        Luau::LanguageServer::AutoImports::InstanceRequireAutoImporterContext importCtx{
            ctx.sourceModule->name,
            ctx.textDocument,
            Luau::NotNull(&workspaceFolder->frontend),
            ctx.workspaceFolder,
            Luau::NotNull(&config.completion.imports),
            hotCommentsLineNumber,
            Luau::NotNull(&importsVisitor),
            Luau::NotNull(this),
            [&unknownSymbol](const auto& variableName)
            {
                return variableName == unknownSymbol.name;
            },
        };

        auto instanceRequires = Luau::LanguageServer::AutoImports::computeAllInstanceRequires(importCtx);
        for (const auto& instanceRequire : instanceRequires)
        {
            lsp::CodeAction action;
            action.title = "Add require for '" + instanceRequire.variableName + "' from \"" + instanceRequire.requirePath + "\"";
            action.kind = lsp::CodeActionKind::QuickFix;
            action.isPreferred = !foundServiceMatch && instanceRequires.size() == 1;

            if (diagnostic)
                action.diagnostics.push_back(*diagnostic);

            lsp::WorkspaceEdit workspaceEdit;
            std::vector<lsp::TextEdit> edits;
            if (instanceRequire.serviceEdit)
                edits.emplace_back(instanceRequire.serviceEdit->second);
            edits.emplace_back(instanceRequire.edit);
            workspaceEdit.changes.emplace(ctx.uri, edits);
            action.edit = workspaceEdit;

            result.push_back(action);
        }
    }
}

std::vector<lsp::TextEdit> RobloxPlatform::computeAddAllMissingImportsEdits(
    const UnknownSymbolFixContext& ctx, const std::vector<Luau::TypeError>& errors)
{
    std::vector<lsp::TextEdit> serviceEdits;
    std::vector<lsp::TextEdit> requireEdits;
    std::set<std::string> addedServices;

    auto config = workspaceFolder->fileResolver.client->getConfiguration(workspaceFolder->rootUri);

    // Find existing imports
    Luau::LanguageServer::AutoImports::RobloxFindImportsVisitor importsVisitor;
    importsVisitor.visit(ctx.sourceModule->root);

    auto hotCommentsLineNumber = Luau::LanguageServer::AutoImports::computeHotCommentsLineNumber(*ctx.sourceModule);

    std::vector<std::string> unknownSymbols;
    for (const auto& error : errors)
    {
        const auto* unknownSymbol = Luau::get_if<Luau::UnknownSymbol>(&error.data);
        if (!unknownSymbol || unknownSymbol->context != Luau::UnknownSymbol::Binding)
            continue;

        unknownSymbols.emplace_back(unknownSymbol->name);
    }

    // Handle symbols as services
    std::optional<RobloxDefinitionsFileMetadata> metadata = workspaceFolder->definitionsFileMetadata;
    auto services = metadata.has_value() ? metadata->SERVICES : std::vector<std::string>{};
    for (const auto& symbolName : unknownSymbols)
    {
        if (std::find(services.begin(), services.end(), symbolName) != services.end())
        {
            if (!contains(importsVisitor.serviceLineMap, symbolName) && !addedServices.count(symbolName))
            {
                size_t lineNumber = importsVisitor.findBestLineForService(symbolName, hotCommentsLineNumber);

                bool appendNewline = false;
                if (config.completion.imports.separateGroupsWithLine && importsVisitor.firstRequireLine &&
                    importsVisitor.firstRequireLine.value() - lineNumber == 0)
                    appendNewline = true;

                serviceEdits.push_back(Luau::LanguageServer::AutoImports::createServiceTextEdit(symbolName, lineNumber, appendNewline));
                addedServices.insert(symbolName);
            }
        }
    }

    // Handle symbol as modules
    if (config.completion.imports.stringRequires.enabled)
    {
        auto baseEdits = LSPPlatform::computeAddAllMissingImportsEdits(ctx, errors);
        for (auto& edit : baseEdits)
            requireEdits.push_back(std::move(edit));
    }
    else
    {
        std::set<std::string> addedRequires;

        Luau::LanguageServer::AutoImports::InstanceRequireAutoImporterContext importCtx{
            ctx.sourceModule->name,
            ctx.textDocument,
            Luau::NotNull(&workspaceFolder->frontend),
            ctx.workspaceFolder,
            Luau::NotNull(&config.completion.imports),
            hotCommentsLineNumber,
            Luau::NotNull(&importsVisitor),
            Luau::NotNull(this),
            [&unknownSymbols](const auto& variableName)
            {
                return contains(unknownSymbols, variableName);
            },
        };

        auto instanceRequires = Luau::LanguageServer::AutoImports::computeAllInstanceRequires(importCtx);

        for (const auto& instanceRequire : instanceRequires)
        {
            if (addedRequires.count(instanceRequire.variableName))
                continue;

            if (instanceRequire.serviceEdit && !addedServices.count(instanceRequire.serviceEdit->first))
            {
                addedServices.insert(instanceRequire.serviceEdit->first);
                serviceEdits.push_back(instanceRequire.serviceEdit->second);
            }

            requireEdits.push_back(instanceRequire.edit);
            addedRequires.insert(instanceRequire.variableName);
        }
    }

    // Combine edits: services first, then requires
    std::sort(serviceEdits.begin(), serviceEdits.end(),
        [](const lsp::TextEdit& a, const lsp::TextEdit& b)
        {
            return a.range.start.line == b.range.start.line ? a.newText < b.newText : a.range.start.line < b.range.start.line;
        });
    std::sort(requireEdits.begin(), requireEdits.end(),
        [](const lsp::TextEdit& a, const lsp::TextEdit& b)
        {
            return a.range.start.line == b.range.start.line ? a.newText < b.newText : a.range.start.line < b.range.start.line;
        });

    std::vector<lsp::TextEdit> allEdits;
    allEdits.reserve(serviceEdits.size() + requireEdits.size());
    for (auto& edit : serviceEdits)
        allEdits.push_back(std::move(edit));
    for (auto& edit : requireEdits)
        allEdits.push_back(std::move(edit));

    return allEdits;
}
