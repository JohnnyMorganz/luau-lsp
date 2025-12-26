#include "Platform/RobloxPlatform.hpp"

#include "LSP/Workspace.hpp"
#include "LSP/Utils.hpp"
#include "Platform/AutoImports.hpp"
#include "Platform/StringRequireAutoImporter.hpp"
#include "Luau/PrettyPrinter.h"

#include <set>

lsp::TextEdit createServiceTextEdit(const std::string& name, size_t lineNumber, bool appendNewline)
{
    auto range = lsp::Range{{lineNumber, 0}, {lineNumber, 0}};
    auto importText = "local " + name + " = game:GetService(\"" + name + "\")\n";
    if (appendNewline)
        importText += "\n";
    return {range, importText};
}

std::string optimiseAbsoluteRequire(const std::string& path)
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

void RobloxPlatform::handleUnknownSymbolFix(const UnknownSymbolFixContext& ctx, const Luau::UnknownSymbol& unknownSymbol,
    const std::optional<lsp::Diagnostic>& diagnostic, std::vector<lsp::CodeAction>& result)
{
    // Only handle binding context (not type references)
    if (unknownSymbol.context != Luau::UnknownSymbol::Binding)
        return;

    ClientConfiguration config;
    if (workspaceFolder->fileResolver.client)
        config = workspaceFolder->fileResolver.client->getConfiguration(workspaceFolder->rootUri);

    // Find existing imports
    RobloxFindImportsVisitor importsVisitor;
    importsVisitor.visit(ctx.sourceModule->root);

    bool hasAction = false;

    // 1. Check if the unknown symbol matches a Roblox service name
    std::optional<RobloxDefinitionsFileMetadata> metadata = workspaceFolder->definitionsFileMetadata;
    auto services = metadata.has_value() ? metadata->SERVICES : std::vector<std::string>{};

    if (std::find(services.begin(), services.end(), unknownSymbol.name) != services.end())
    {
        // Check if already imported
        if (!contains(importsVisitor.serviceLineMap, unknownSymbol.name))
        {
            size_t lineNumber = importsVisitor.findBestLineForService(unknownSymbol.name, ctx.hotCommentsLineNumber);

            bool appendNewline = false;
            if (config.completion.imports.separateGroupsWithLine && importsVisitor.firstRequireLine &&
                importsVisitor.firstRequireLine.value() - lineNumber == 0)
                appendNewline = true;

            auto serviceEdit = createServiceTextEdit(unknownSymbol.name, lineNumber, appendNewline);

            lsp::CodeAction action;
            action.title = "Import service '" + unknownSymbol.name + "'";
            action.kind = lsp::CodeActionKind::QuickFix;
            action.isPreferred = !hasAction; // First action is preferred

            if (diagnostic)
                action.diagnostics.push_back(*diagnostic);

            lsp::WorkspaceEdit workspaceEdit;
            workspaceEdit.changes.emplace(ctx.uri, std::vector{serviceEdit});
            action.edit = workspaceEdit;

            result.push_back(action);
            hasAction = true;
        }
    }

    // 2. Check for modules that can be required
    if (config.completion.imports.stringRequires.enabled)
    {
        // Delegate to base implementation for string requires
        LSPPlatform::handleUnknownSymbolFix(ctx, unknownSymbol, diagnostic, result);
    }
    else
    {
        // Generate script-path requires
        size_t minimumLineNumber =
            Luau::LanguageServer::AutoImports::computeMinimumLineNumberForRequire(importsVisitor, ctx.hotCommentsLineNumber);

        for (auto& [path, node] : virtualPathsToSourceNodes)
        {
            auto name = Luau::LanguageServer::AutoImports::makeValidVariableName(node->name);

            if (name != unknownSymbol.name)
                continue;

            if (path == ctx.sourceModule->name || node->className != "ModuleScript" || importsVisitor.containsRequire(name))
                continue;

            if (auto scriptFilePath = getRealPathFromSourceNode(node);
                scriptFilePath && workspaceFolder->isIgnoredFileForAutoImports(*scriptFilePath, config))
                continue;

            std::string requirePath;
            std::vector<lsp::TextEdit> textEdits;

            // Compute the style of require
            bool isRelative = false;
            auto parent1 = getParentPath(ctx.sourceModule->name), parent2 = getParentPath(path);
            if (config.completion.imports.requireStyle == ImportRequireStyle::AlwaysRelative ||
                Luau::startsWith(path, "ProjectRoot/") || // All model projects should always require relatively
                (config.completion.imports.requireStyle != ImportRequireStyle::AlwaysAbsolute &&
                    (Luau::startsWith(ctx.sourceModule->name, path) || Luau::startsWith(path, ctx.sourceModule->name) || parent1 == parent2)))
            {
                // HACK: using Uri's purely to access lexicallyRelative
                requirePath = "./" + Uri::file(path).lexicallyRelative(Uri::file(ctx.sourceModule->name));
                isRelative = true;
            }
            else
                requirePath = optimiseAbsoluteRequire(path);

            auto require = convertToScriptPath(requirePath);

            size_t lineNumber =
                Luau::LanguageServer::AutoImports::computeBestLineForRequire(importsVisitor, *ctx.textDocument, require, minimumLineNumber);

            if (!isRelative)
            {
                // Service will be the first part of the path
                // If we haven't imported the service already, then we auto-import it
                auto service = requirePath.substr(0, requirePath.find('/'));
                if (!contains(importsVisitor.serviceLineMap, service))
                {
                    auto serviceLineNumber = importsVisitor.findBestLineForService(service, ctx.hotCommentsLineNumber);
                    bool appendNewline = false;
                    if (config.completion.imports.separateGroupsWithLine &&
                        importsVisitor.firstRequireLine.value_or(serviceLineNumber) - serviceLineNumber == 0)
                        appendNewline = true;
                    textEdits.emplace_back(createServiceTextEdit(service, serviceLineNumber, appendNewline));
                }
            }

            // Whether we need to add a newline before the require to separate it from the services
            bool prependNewline = config.completion.imports.separateGroupsWithLine && importsVisitor.shouldPrependNewline(lineNumber);
            textEdits.emplace_back(Luau::LanguageServer::AutoImports::createRequireTextEdit(name, require, lineNumber, prependNewline));

            lsp::CodeAction action;
            action.title = "Add require for '" + name + "' from \"" + require + "\"";
            action.kind = lsp::CodeActionKind::QuickFix;
            action.isPreferred = !hasAction; // First action is preferred

            if (diagnostic)
                action.diagnostics.push_back(*diagnostic);

            lsp::WorkspaceEdit workspaceEdit;
            workspaceEdit.changes.emplace(ctx.uri, textEdits);
            action.edit = workspaceEdit;

            result.push_back(action);
            hasAction = true;
        }
    }
}

std::vector<lsp::TextEdit> RobloxPlatform::computeAddAllMissingImportsEdits(
    const UnknownSymbolFixContext& ctx, const std::vector<Luau::TypeError>& errors)
{
    std::vector<lsp::TextEdit> serviceEdits;
    std::vector<lsp::TextEdit> requireEdits;
    std::set<std::string> addedServices;
    std::set<std::string> addedRequires;

    auto config = workspaceFolder->fileResolver.client->getConfiguration(workspaceFolder->rootUri);

    // Find existing imports
    RobloxFindImportsVisitor importsVisitor;
    importsVisitor.visit(ctx.sourceModule->root);

    std::optional<RobloxDefinitionsFileMetadata> metadata = workspaceFolder->definitionsFileMetadata;
    auto services = metadata.has_value() ? metadata->SERVICES : std::vector<std::string>{};

    size_t minimumLineNumber =
        Luau::LanguageServer::AutoImports::computeMinimumLineNumberForRequire(importsVisitor, ctx.hotCommentsLineNumber);

    for (const auto& error : errors)
    {
        const auto* unknownSymbol = Luau::get_if<Luau::UnknownSymbol>(&error.data);
        if (!unknownSymbol || unknownSymbol->context != Luau::UnknownSymbol::Binding)
            continue;

        const std::string& symbolName = unknownSymbol->name;

        // 1. Check if it's a service
        if (std::find(services.begin(), services.end(), symbolName) != services.end())
        {
            if (!contains(importsVisitor.serviceLineMap, symbolName) && !addedServices.count(symbolName))
            {
                size_t lineNumber = importsVisitor.findBestLineForService(symbolName, ctx.hotCommentsLineNumber);

                bool appendNewline = false;
                if (config.completion.imports.separateGroupsWithLine && importsVisitor.firstRequireLine &&
                    importsVisitor.firstRequireLine.value() - lineNumber == 0)
                    appendNewline = true;

                serviceEdits.push_back(createServiceTextEdit(symbolName, lineNumber, appendNewline));
                addedServices.insert(symbolName);
            }
            continue;
        }

        // Skip if we've already added a require for this name
        if (addedRequires.count(symbolName))
            continue;

        // Skip if already imported
        if (importsVisitor.containsRequire(symbolName))
            continue;

        // 2. Check for modules - use string requires if enabled
        if (config.completion.imports.stringRequires.enabled)
        {
            // Delegate to base implementation
            auto baseEdits = LSPPlatform::computeAddAllMissingImportsEdits(ctx, {error});
            for (auto& edit : baseEdits)
                requireEdits.push_back(std::move(edit));
            if (!baseEdits.empty())
                addedRequires.insert(symbolName);
        }
        else
        {
            // Generate script-path requires
            for (auto& [path, node] : virtualPathsToSourceNodes)
            {
                auto name = Luau::LanguageServer::AutoImports::makeValidVariableName(node->name);

                if (name != symbolName)
                    continue;

                if (path == ctx.sourceModule->name || node->className != "ModuleScript" || importsVisitor.containsRequire(name))
                    continue;

                if (auto scriptFilePath = getRealPathFromSourceNode(node);
                    scriptFilePath && workspaceFolder->isIgnoredFileForAutoImports(*scriptFilePath, config))
                    continue;

                std::string requirePath;

                // Compute the style of require
                bool isRelative = false;
                auto parent1 = getParentPath(ctx.sourceModule->name), parent2 = getParentPath(path);
                if (config.completion.imports.requireStyle == ImportRequireStyle::AlwaysRelative ||
                    Luau::startsWith(path, "ProjectRoot/") ||
                    (config.completion.imports.requireStyle != ImportRequireStyle::AlwaysAbsolute &&
                        (Luau::startsWith(ctx.sourceModule->name, path) || Luau::startsWith(path, ctx.sourceModule->name) || parent1 == parent2)))
                {
                    requirePath = "./" + Uri::file(path).lexicallyRelative(Uri::file(ctx.sourceModule->name));
                    isRelative = true;
                }
                else
                    requirePath = optimiseAbsoluteRequire(path);

                auto require = convertToScriptPath(requirePath);

                size_t lineNumber =
                    Luau::LanguageServer::AutoImports::computeBestLineForRequire(importsVisitor, *ctx.textDocument, require, minimumLineNumber);

                if (!isRelative)
                {
                    // Service will be the first part of the path
                    // If we haven't imported the service already, then we auto-import it
                    auto service = requirePath.substr(0, requirePath.find('/'));
                    if (!contains(importsVisitor.serviceLineMap, service) && !addedServices.count(service))
                    {
                        auto serviceLineNumber = importsVisitor.findBestLineForService(service, ctx.hotCommentsLineNumber);
                        bool appendNewline = false;
                        if (config.completion.imports.separateGroupsWithLine &&
                            importsVisitor.firstRequireLine.value_or(serviceLineNumber) - serviceLineNumber == 0)
                            appendNewline = true;
                        serviceEdits.push_back(createServiceTextEdit(service, serviceLineNumber, appendNewline));
                        addedServices.insert(service);
                    }
                }

                bool prependNewline = config.completion.imports.separateGroupsWithLine && importsVisitor.shouldPrependNewline(lineNumber);
                requireEdits.push_back(Luau::LanguageServer::AutoImports::createRequireTextEdit(name, require, lineNumber, prependNewline));
                addedRequires.insert(name);
                break; // Use first match
            }
        }
    }

    // Combine edits: services first, then requires
    std::vector<lsp::TextEdit> allEdits;
    allEdits.reserve(serviceEdits.size() + requireEdits.size());
    for (auto& edit : serviceEdits)
        allEdits.push_back(std::move(edit));
    for (auto& edit : requireEdits)
        allEdits.push_back(std::move(edit));

    return allEdits;
}
