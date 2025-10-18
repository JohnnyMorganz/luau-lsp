#include "Platform/RobloxPlatform.hpp"

#include "LSP/Workspace.hpp"
#include "Luau/PrettyPrinter.h"

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
    RobloxFindImportsVisitor visitor;
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
