#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"
#include "Protocol/CodeAction.hpp"
#include "LSP/LuauExt.hpp"
#include "Luau/Transpiler.h"

lsp::WorkspaceEdit WorkspaceFolder::computeOrganiseRequiresEdit(const lsp::DocumentUri& uri)
{
    auto moduleName = fileResolver.getModuleName(uri);
    auto textDocument = fileResolver.getTextDocument(uri);

    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + uri.toString());

    if (frontend.isDirty(moduleName))
        frontend.check(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return {};

    // Find all the `local X = require(...)` calls
    FindRequiresVisitor visitor;
    visitor.visit(sourceModule->root);

    // Check if there are any requires
    if (visitor.requiresMap.size() == 1 && visitor.requiresMap.back().empty())
        return {};

    // Treat each require group individually
    std::vector<lsp::TextEdit> edits;
    for (const auto& requireGroup : visitor.requiresMap)
    {
        if (requireGroup.empty())
            continue;

        // Find the first line of the group
        size_t firstRequireLine = requireGroup.begin()->second->location.begin.line;
        Luau::Location previousRequireLocation{{0, 0}, {0, 0}};
        bool isSorted = true;
        for (const auto& [_, stat] : requireGroup)
        {
            if (stat->location.begin < previousRequireLocation.begin)
                isSorted = false;
            previousRequireLocation = stat->location;
            firstRequireLine = stat->location.begin.line < firstRequireLine ? stat->location.begin.line : firstRequireLine;
        }

        // Test to see that if all the requires are already sorted -> if they are, then just leave alone
        // to prevent clogging the undo history stack
        if (isSorted)
            continue;

        // We firstly delete all the previous requires, as they will be added later
        for (const auto& [_, stat] : requireGroup)
            edits.emplace_back(lsp::TextEdit{{{stat->location.begin.line, 0}, {stat->location.begin.line + 1, 0}}, ""});

        // We find the first line to add these services to, and then add them in sorted order
        lsp::Range insertLocation{{firstRequireLine, 0}, {firstRequireLine, 0}};
        for (const auto& [serviceName, stat] : requireGroup)
        {
            // We need to rewrite the statement as we expected it
            auto importText = Luau::toString(stat) + "\n";
            edits.emplace_back(lsp::TextEdit{insertLocation, importText});
        }
    }

    lsp::WorkspaceEdit workspaceEdit;
    workspaceEdit.changes.emplace(uri.toString(), edits);
    return workspaceEdit;
}

lsp::WorkspaceEdit WorkspaceFolder::computeOrganiseServicesEdit(const lsp::DocumentUri& uri)
{
    auto moduleName = fileResolver.getModuleName(uri);
    auto textDocument = fileResolver.getTextDocument(uri);

    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + uri.toString());

    if (frontend.isDirty(moduleName))
        frontend.check(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return {};

    // Find all the `local X = game:GetService("Service")` calls
    FindServicesVisitor visitor;
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
    workspaceEdit.changes.emplace(uri.toString(), edits);
    return workspaceEdit;
}

lsp::CodeActionResult WorkspaceFolder::codeAction(const lsp::CodeActionParams& params)
{
    std::vector<lsp::CodeAction> result;

    auto config = client->getConfiguration(rootUri);

    // Add organise imports code action
    if (params.context.only.empty() || contains(params.context.only, lsp::CodeActionKind::Source) ||
        contains(params.context.only, lsp::CodeActionKind::SourceOrganizeImports))
    {
        // Add sort requires code action
        lsp::CodeAction organiseImportsAction;
        organiseImportsAction.title = "Sort requires";
        organiseImportsAction.kind = lsp::CodeActionKind::SourceOrganizeImports;

        // TODO: support resolving and defer computation till later
        // if (client->capabilities.textDocument && client->capabilities.textDocument->codeAction &&
        //     client->capabilities.textDocument->codeAction->resolveSupport &&
        //     contains(client->capabilities.textDocument->codeAction->resolveSupport->properties, "edit"))
        organiseImportsAction.edit = computeOrganiseRequiresEdit(params.textDocument.uri);
        result.emplace_back(organiseImportsAction);

        // If in Roblox mode, add a sort services code action
        if (config.types.roblox)
        {
            lsp::CodeAction sortServicesAction;
            sortServicesAction.title = "Sort services";
            sortServicesAction.kind = lsp::CodeActionKind::SourceOrganizeImports;
            sortServicesAction.edit = computeOrganiseServicesEdit(params.textDocument.uri);
            result.emplace_back(sortServicesAction);
        }
    }

    return result;
}

lsp::CodeActionResult LanguageServer::codeAction(const lsp::CodeActionParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->codeAction(params);
}