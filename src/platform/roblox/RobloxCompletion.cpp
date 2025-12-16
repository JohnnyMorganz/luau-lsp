#include "Platform/RobloxPlatform.hpp"
#include "Platform/RotrieverResolver.hpp"

#include "Luau/TimeTrace.h"
#include "LuauFileUtils.hpp"

#include "LSP/Completion.hpp"

#include "Platform/AutoImports.hpp"
#include "Platform/StringRequireAutoImporter.hpp"

LUAU_FASTFLAG(LuauSolverV2)

static constexpr const char* COMMON_SERVICES[] = {
    "Players",
    "ReplicatedStorage",
    "ServerStorage",
    "MessagingService",
    "TeleportService",
    "HttpService",
    "CollectionService",
    "DataStoreService",
    "ContextActionService",
    "UserInputService",
    "Teams",
    "Chat",
    "TextService",
    "TextChatService",
    "GamepadService",
    "VoiceChatService",
};

static constexpr const char* COMMON_INSTANCE_PROPERTIES[] = {
    "Parent",
    "Name",
    // Methods
    "FindFirstChild",
    "IsA",
    "Destroy",
    "GetAttribute",
    "GetChildren",
    "GetDescendants",
    "WaitForChild",
    "Clone",
    "SetAttribute",
};

static constexpr const char* COMMON_SERVICE_PROVIDER_PROPERTIES[] = {
    "GetService",
};

static lsp::TextEdit createServiceTextEdit(const std::string& name, size_t lineNumber, bool appendNewline = false)
{
    auto range = lsp::Range{{lineNumber, 0}, {lineNumber, 0}};
    auto importText = "local " + name + " = game:GetService(\"" + name + "\")\n";
    if (appendNewline)
        importText += "\n";
    return {range, importText};
}

static lsp::CompletionItem createSuggestService(const std::string& service, size_t lineNumber, bool appendNewline = false)
{
    auto textEdit = createServiceTextEdit(service, lineNumber, appendNewline);

    lsp::CompletionItem item;
    item.label = service;
    item.kind = lsp::CompletionItemKind::Class;
    item.detail = "Auto-import";
    item.documentation = {lsp::MarkupKind::Markdown, codeBlock("luau", textEdit.newText)};
    item.insertText = service;
    item.sortText = SortText::AutoImports;

    item.additionalTextEdits.emplace_back(textEdit);

    return item;
}

static std::string optimiseAbsoluteRequire(const std::string& path)
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

std::optional<Luau::AutocompleteEntryMap> RobloxPlatform::completionCallback(
    const std::string& tag, std::optional<const Luau::ExternType*> ctx, std::optional<std::string> contents, const Luau::ModuleName& moduleName)
{
    if (auto parentResult = LSPPlatform::completionCallback(tag, ctx, contents, moduleName))
        return parentResult;

    std::optional<RobloxDefinitionsFileMetadata> metadata = workspaceFolder->definitionsFileMetadata;

    if (tag == "ClassNames")
    {
        if (auto instanceType = workspaceFolder->frontend.globals.globalScope->lookupType("Instance"))
        {
            if (auto* ctv = Luau::get<Luau::ExternType>(instanceType->type))
            {
                Luau::AutocompleteEntryMap result;
                for (auto& [_, ty] : workspaceFolder->frontend.globals.globalScope->exportedTypeBindings)
                {
                    if (auto* c = Luau::get<Luau::ExternType>(ty.type))
                    {
                        // Check if the ctv is a subclass of instance
                        if (Luau::isSubclass(c, ctv))

                            result.insert_or_assign(
                                c->name, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String,
                                             workspaceFolder->frontend.builtinTypes->stringType, false, false, Luau::TypeCorrectKind::Correct});
                    }
                }

                return result;
            }
        }
    }
    else if (tag == "Properties")
    {
        if (ctx && ctx.value())
        {
            Luau::AutocompleteEntryMap result;
            auto ctv = ctx.value();
            while (ctv)
            {
                for (auto& [propName, prop] : ctv->props)
                {
                    // Don't include functions or events
                    LUAU_ASSERT(prop.readTy);
                    auto ty = Luau::follow(*prop.readTy);
                    if (Luau::get<Luau::FunctionType>(ty) || Luau::isOverloadedFunction(ty))
                        continue;
                    else if (auto ttv = Luau::get<Luau::TableType>(ty); ttv && ttv->name && ttv->name.value() == "RBXScriptSignal")
                        continue;
                    else if (Luau::hasTag(prop, kSourcemapGeneratedTag))
                        continue;

                    result.insert_or_assign(
                        propName, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType,
                                      false, false, Luau::TypeCorrectKind::Correct});
                }
                if (ctv->parent)
                    ctv = Luau::get<Luau::ExternType>(*ctv->parent);
                else
                    break;
            }
            return result;
        }
    }
    else if (tag == "Children")
    {
        if (auto ctv = ctx.value())
        {
            Luau::AutocompleteEntryMap result;
            for (auto& [propName, prop] : ctv->props)
            {
                if (Luau::hasTag(prop, kSourcemapGeneratedTag) &&
                    !(prop.readTy && (Luau::is<Luau::FunctionType>(*prop.readTy) || Luau::isOverloadedFunction(*prop.readTy))))
                    result.insert_or_assign(
                        propName, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType,
                                      false, false, Luau::TypeCorrectKind::Correct});
            }
            return result;
        }
    }
    else if (tag == "Enums")
    {
        auto it = workspaceFolder->frontend.globals.globalScope->importedTypeBindings.find("Enum");
        if (it == workspaceFolder->frontend.globals.globalScope->importedTypeBindings.end())
            return std::nullopt;

        Luau::AutocompleteEntryMap result;
        for (auto& [enumName, _] : it->second)
            result.insert_or_assign(enumName, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String,
                                                  workspaceFolder->frontend.builtinTypes->stringType, false, false, Luau::TypeCorrectKind::Correct});

        return result;
    }
    else if (tag == "CreatableInstances")
    {
        Luau::AutocompleteEntryMap result;
        if (metadata)
        {
            for (const auto& className : metadata->CREATABLE_INSTANCES)
                result.insert_or_assign(
                    className, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType, false,
                                   false, Luau::TypeCorrectKind::Correct});
        }
        return result;
    }
    else if (tag == "Services")
    {
        Luau::AutocompleteEntryMap result;

        // We are autocompleting a `game:GetService("$1")` call, so we set a flag to
        // highlight this so that we can prioritise common services first in the list
        if (metadata)
        {
            for (const auto& className : metadata->SERVICES)
                result.insert_or_assign(
                    className, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType, false,
                                   false, Luau::TypeCorrectKind::Correct});
        }

        return result;
    }

    return std::nullopt;
}

const char* RobloxPlatform::handleSortText(
    const Luau::Frontend& frontend, const std::string& name, const Luau::AutocompleteEntry& entry, const std::unordered_set<std::string>& tags)
{
    // If it's a `game:GetSerivce("$1")` call, then prioritise common services
    if (tags.count("Services"))
        if (auto it = std::find(std::begin(COMMON_SERVICES), std::end(COMMON_SERVICES), name); it != std::end(COMMON_SERVICES))
            return SortText::PrioritisedSuggestion;

    // If calling a property on ServiceProvider, then prioritise these properties
    auto& completionGlobals = FFlag::LuauSolverV2 ? frontend.globals : frontend.globalsForAutocomplete;
    if (auto dataModelType = completionGlobals.globalScope->lookupType("ServiceProvider");
        dataModelType && Luau::get<Luau::ExternType>(dataModelType->type) && entry.containingExternType &&
        Luau::isSubclass(entry.containingExternType.value(), Luau::get<Luau::ExternType>(dataModelType->type)) && !entry.wrongIndexType)
    {
        if (auto it = std::find(std::begin(COMMON_SERVICE_PROVIDER_PROPERTIES), std::end(COMMON_SERVICE_PROVIDER_PROPERTIES), name);
            it != std::end(COMMON_SERVICE_PROVIDER_PROPERTIES))
            return SortText::PrioritisedSuggestion;
    }

    // If calling a property on an Instance, then prioritise these properties
    else if (auto instanceType = completionGlobals.globalScope->lookupType("Instance");
        instanceType && Luau::get<Luau::ExternType>(instanceType->type) && entry.containingExternType &&
        Luau::isSubclass(entry.containingExternType.value(), Luau::get<Luau::ExternType>(instanceType->type)) && !entry.wrongIndexType)
    {
        if (auto it = std::find(std::begin(COMMON_INSTANCE_PROPERTIES), std::end(COMMON_INSTANCE_PROPERTIES), name);
            it != std::end(COMMON_INSTANCE_PROPERTIES))
            return SortText::PrioritisedSuggestion;
    }

    return nullptr;
}

std::optional<lsp::CompletionItemKind> RobloxPlatform::handleEntryKind(const Luau::AutocompleteEntry& entry)
{
    if (entry.type.has_value())
    {
        auto id = Luau::follow(entry.type.value());

        if (auto ttv = Luau::get<Luau::TableType>(id))
        {
            // Special case the RBXScriptSignal type as a connection
            if (ttv->name && ttv->name.value() == "RBXScriptSignal")
                return lsp::CompletionItemKind::Event;
        }
    }

    return std::nullopt;
}

// Create text edits to set up the Packages local variable
// Returns: the text edits to insert, and the number of lines added
static std::pair<std::vector<lsp::TextEdit>, size_t> createPackagesSetupEdits(
    const std::string& currentPackageName, const std::string& packagesLocalName, size_t insertLineNumber)
{
    // Insert all setup text at a single point (beginning of insertLineNumber)
    // This avoids issues with line number shifts during multi-line edits
    std::string setupText;
    setupText += "local " + currentPackageName + " = script:FindFirstAncestor(\"" + currentPackageName + "\")\n";
    setupText += "local " + packagesLocalName + " = " + currentPackageName + ".Parent\n";
    setupText += "\n"; // Blank line before requires

    std::vector<lsp::TextEdit> edits;
    edits.push_back({lsp::Range{{insertLineNumber, 0}, {insertLineNumber, 0}}, setupText});

    // Setup adds 3 lines: FindFirstAncestor, Packages = X.Parent, blank line
    return {edits, 3};
}

// Create: local PackageName = require(Packages.PackageName)
static lsp::CompletionItem createRotrieverPackageSuggestion(const std::string& packageName, const std::string& packagesLocalName, size_t lineNumber,
    bool prependNewline, const std::vector<lsp::TextEdit>& setupEdits = {})
{
    auto requirePath = packagesLocalName + "." + packageName;
    auto importText = "local " + packageName + " = require(" + requirePath + ")\n";
    if (prependNewline)
        importText = "\n" + importText;

    auto range = lsp::Range{{lineNumber, 0}, {lineNumber, 0}};
    lsp::TextEdit textEdit{range, importText};

    lsp::CompletionItem item;
    item.label = packageName;
    item.kind = lsp::CompletionItemKind::Module;
    item.detail = "Auto-import package";

    // Build documentation showing all the edits
    std::string docText;
    for (const auto& edit : setupEdits)
        docText += edit.newText;
    docText += textEdit.newText;
    item.documentation = {lsp::MarkupKind::Markdown, codeBlock("luau", docText)};

    item.insertText = packageName;
    item.sortText = SortText::AutoImportsRotriever;

    // Add setup edits first, then the require edit
    for (const auto& edit : setupEdits)
        item.additionalTextEdits.emplace_back(edit);
    item.additionalTextEdits.emplace_back(textEdit);

    return item;
}

// Create: local ExportName = require(Packages.PackageName).ExportPath
// Or if package already imported: local ExportName = PackageName.ExportPath
// exportPath can be nested like "Hooks.useAbbreviatedNumberLocalization"
static lsp::CompletionItem createRotrieverExportSuggestion(const std::string& exportPath, const std::string& packageName,
    const std::string& packagesLocalName, size_t lineNumber, bool prependNewline, bool packageAlreadyImported,
    const std::vector<lsp::TextEdit>& setupEdits = {})
{
    // For nested exports like "Hooks.useAbbreviatedNumberLocalization", the local name is the last part
    std::string localName = exportPath;
    if (auto lastDot = exportPath.rfind('.'); lastDot != std::string::npos)
        localName = exportPath.substr(lastDot + 1);

    std::string importText;
    if (prependNewline)
        importText = "\n";

    if (packageAlreadyImported)
    {
        // Package already imported, just reference it directly
        // local useAbbreviatedNumberLocalization = Localization.Hooks.useAbbreviatedNumberLocalization
        importText += "local " + localName + " = " + packageName + "." + exportPath + "\n";
    }
    else
    {
        // Need to require the package
        // local useAbbreviatedNumberLocalization = require(Packages.Localization).Hooks.useAbbreviatedNumberLocalization
        auto requirePath = packagesLocalName + "." + packageName;
        importText += "local " + localName + " = require(" + requirePath + ")." + exportPath + "\n";
    }

    auto range = lsp::Range{{lineNumber, 0}, {lineNumber, 0}};
    lsp::TextEdit textEdit{range, importText};

    lsp::CompletionItem item;
    item.label = localName;
    item.kind = lsp::CompletionItemKind::Module;
    // Show the full path in detail for nested exports
    item.detail = exportPath == localName ? ("Auto-import from " + packageName) : ("Auto-import " + packageName + "." + exportPath);

    // Build documentation showing all the edits
    std::string docText;
    for (const auto& edit : setupEdits)
        docText += edit.newText;
    docText += textEdit.newText;
    item.documentation = {lsp::MarkupKind::Markdown, codeBlock("luau", docText)};

    item.insertText = localName;
    item.sortText = SortText::AutoImportsRotriever;

    // Add setup edits first, then the require edit
    for (const auto& edit : setupEdits)
        item.additionalTextEdits.emplace_back(edit);
    item.additionalTextEdits.emplace_back(textEdit);

    return item;
}

// Create type import using two-line pattern (required for valid Luau syntax):
//   local PackageName = require(Packages.PackageName)
//   type TypeName = PackageName.TypePath
// typePath can be nested like "RoduxTypes.GameInfoModel" -> type GameInfoModel = PackageName.RoduxTypes.GameInfoModel
static lsp::CompletionItem createRotrieverTypeExportSuggestion(const std::string& typePath, const std::string& packageName,
    const std::string& packagesLocalName, size_t lineNumber, bool prependNewline, bool packageAlreadyImported,
    const std::vector<lsp::TextEdit>& setupEdits = {})
{
    // For nested types like "RoduxTypes.GameInfoModel", the local name is the last part
    std::string localName = typePath;
    if (auto lastDot = typePath.rfind('.'); lastDot != std::string::npos)
        localName = typePath.substr(lastDot + 1);

    auto requirePath = packagesLocalName + "." + packageName;

    // Build the import text - two lines for valid Luau syntax:
    // local PackageName = require(Packages.PackageName)
    // type TypeName = PackageName.TypePath
    std::string importText;
    if (prependNewline)
        importText = "\n";

    // Only add the require line if the package isn't already imported
    if (!packageAlreadyImported)
        importText += "local " + packageName + " = require(" + requirePath + ")\n";

    importText += "type " + localName + " = " + packageName + "." + typePath + "\n";

    auto range = lsp::Range{{lineNumber, 0}, {lineNumber, 0}};
    lsp::TextEdit textEdit{range, importText};

    lsp::CompletionItem item;
    item.label = localName;
    item.kind = lsp::CompletionItemKind::Interface; // Use Interface for types
    // Show the full path in detail for nested types
    item.detail = typePath == localName ? ("Auto-import type from " + packageName) : ("Auto-import type " + packageName + "." + typePath);

    // Build documentation showing all the edits
    std::string docText;
    for (const auto& edit : setupEdits)
        docText += edit.newText;
    docText += textEdit.newText;
    item.documentation = {lsp::MarkupKind::Markdown, codeBlock("luau", docText)};

    item.insertText = localName;
    item.sortText = SortText::AutoImportsRotriever;

    // Add setup edits first, then the type import edit
    for (const auto& edit : setupEdits)
        item.additionalTextEdits.emplace_back(edit);
    item.additionalTextEdits.emplace_back(textEdit);

    return item;
}

void RobloxPlatform::handleSuggestImports(const TextDocument& textDocument, const Luau::SourceModule& module, const ClientConfiguration& config,
    size_t hotCommentsLineNumber, bool completingTypeReferencePrefix, std::vector<lsp::CompletionItem>& items)
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::handleSuggestImports", "LSP");

    // Find all import calls
    RobloxFindImportsVisitor importsVisitor;
    importsVisitor.client = workspaceFolder->client;
    importsVisitor.visit(module.root);

    if (config.completion.imports.suggestServices && !completingTypeReferencePrefix)
    {
        std::optional<RobloxDefinitionsFileMetadata> metadata = workspaceFolder->definitionsFileMetadata;

        auto services = metadata.has_value() ? metadata->SERVICES : std::vector<std::string>{};
        for (auto& service : services)
        {
            // ASSUMPTION: if the service was defined, it was defined with the exact same name
            if (contains(importsVisitor.serviceLineMap, service))
                continue;

            if ((!config.completion.imports.includedServices.empty() && !contains(config.completion.imports.includedServices, service)) ||
                contains(config.completion.imports.excludedServices, service))
                continue;

            size_t lineNumber = importsVisitor.findBestLineForService(service, hotCommentsLineNumber);

            bool appendNewline = false;
            if (config.completion.imports.separateGroupsWithLine && importsVisitor.firstRequireLine &&
                importsVisitor.firstRequireLine.value() - lineNumber == 0)
                appendNewline = true;

            items.emplace_back(createSuggestService(service, lineNumber, appendNewline));
        }
    }

    if (config.completion.imports.suggestRequires)
    {
        if (config.completion.imports.stringRequires.enabled)
        {
            Luau::LanguageServer::AutoImports::StringRequireAutoImporterContext ctx{
                module.name,
                Luau::NotNull(&textDocument),
                Luau::NotNull(&workspaceFolder->frontend),
                Luau::NotNull(workspaceFolder),
                Luau::NotNull(&config.completion.imports),
                hotCommentsLineNumber,
                Luau::NotNull(&importsVisitor),
            };

            return Luau::LanguageServer::AutoImports::suggestStringRequires(ctx, items);
        }
        else
        {
            size_t minimumLineNumber = computeMinimumLineNumberForRequire(importsVisitor, hotCommentsLineNumber);

            for (auto& [path, node] : virtualPathsToSourceNodes)
            {
                auto name = Luau::LanguageServer::AutoImports::makeValidVariableName(node->name);

                if (path == module.name || node->className != "ModuleScript" || importsVisitor.containsRequire(name))
                    continue;
                if (auto scriptFilePath = getRealPathFromSourceNode(node);
                    scriptFilePath && workspaceFolder->isIgnoredFileForAutoImports(*scriptFilePath, config))
                    continue;

                // // Skip _Workspace paths - these are internal paths that usually have shorter aliases
                // // e.g., CorePackages/Workspace/Packages/_Workspace/X/Y -> CorePackages/Packages/Y
                // if (path.find("/_Workspace/") != std::string::npos)
                //     continue;

                std::string requirePath;
                std::vector<lsp::TextEdit> textEdits;

                // Compute the style of require
                bool isRelative = false;
                auto parent1 = getParentPath(module.name), parent2 = getParentPath(path);
                if (config.completion.imports.requireStyle == ImportRequireStyle::AlwaysRelative ||
                    Luau::startsWith(path, "ProjectRoot/") || // All model projects should always require relatively
                    (config.completion.imports.requireStyle != ImportRequireStyle::AlwaysAbsolute &&
                        (Luau::startsWith(module.name, path) || Luau::startsWith(path, module.name) || parent1 == parent2)))
                {
                    // HACK: using Uri's purely to access lexicallyRelative
                    requirePath = "./" + Uri::file(path).lexicallyRelative(Uri::file(module.name));
                    isRelative = true;
                }
                else
                    requirePath = optimiseAbsoluteRequire(path);

                auto require = convertToScriptPath(requirePath);

                size_t lineNumber = computeBestLineForRequire(importsVisitor, textDocument, require, minimumLineNumber);

                if (!isRelative)
                {
                    // Service will be the first part of the path
                    // If we haven't imported the service already, then we auto-import it
                    auto service = requirePath.substr(0, requirePath.find('/'));
                    if (!contains(importsVisitor.serviceLineMap, service))
                    {
                        auto serviceLineNumber = importsVisitor.findBestLineForService(service, hotCommentsLineNumber);
                        bool appendNewline = false;
                        // If there is no firstRequireLine, then the require that we insert will become the first require,
                        // so we use `.value_or(serviceLineNumber)` to ensure it equals 0 and a newline is added
                        if (config.completion.imports.separateGroupsWithLine &&
                            importsVisitor.firstRequireLine.value_or(serviceLineNumber) - serviceLineNumber == 0)
                            appendNewline = true;
                        textEdits.emplace_back(createServiceTextEdit(service, serviceLineNumber, appendNewline));
                    }
                }

                // Whether we need to add a newline before the require to separate it from the services
                bool prependNewline = config.completion.imports.separateGroupsWithLine && importsVisitor.shouldPrependNewline(lineNumber);

                textEdits.emplace_back(Luau::LanguageServer::AutoImports::createRequireTextEdit(name, require, lineNumber, prependNewline));

                items.emplace_back(Luau::LanguageServer::AutoImports::createSuggestRequire(
                    name, textEdits, isRelative ? SortText::AutoImports : SortText::AutoImportsAbsolute, path, require));
            }
        }
    }

    // Suggest Rotriever package imports (prioritized over regular auto-imports)
    // Check if enabled and if the current file is in a Rotriever package
    if (config.completion.imports.suggestRotrieverImports)
    {
        if (auto* currentPackage = workspaceFolder->findRotrieverPackageForFile(textDocument.uri()))
        {
            size_t minimumLineNumber = computeMinimumLineNumberForRequire(importsVisitor, hotCommentsLineNumber);

            // Check if the file already has a "Packages" local variable defined
            // Use the detected name, or default to "Packages"
            std::string packagesLocalName = importsVisitor.hasPackagesLocal() ? importsVisitor.packagesLocalName : "Packages";

            // Setup edits needed if Packages local doesn't exist
            std::vector<lsp::TextEdit> setupEdits;
            size_t setupLineOffset = 0;
            size_t setupInsertLine = 0;

            if (!importsVisitor.hasPackagesLocal())
            {
                // Need to insert the Packages setup BEFORE any existing requires
                // The setup line should be:
                // - After hot comments
                // - After services (if any)
                // - BEFORE any existing requires
                setupInsertLine = hotCommentsLineNumber;
                size_t visitorMinLine = importsVisitor.getMinimumRequireLine();
                if (visitorMinLine > setupInsertLine)
                    setupInsertLine = visitorMinLine;
                // Note: we intentionally don't include firstRequireLine here - setup goes BEFORE requires

                auto [edits, linesAdded] = createPackagesSetupEdits(currentPackage->name, packagesLocalName, setupInsertLine);
                setupEdits = std::move(edits);
                setupLineOffset = linesAdded;
            }

            // For each dependency of this package, suggest the package and its exports
            for (const auto& [depName, dep] : currentPackage->dependencies)
            {
                size_t lineNumber = computeBestLineForRequire(importsVisitor, textDocument, depName, minimumLineNumber);
                // Adjust line number if we're inserting setup edits before the requires
                if (!importsVisitor.hasPackagesLocal() && lineNumber >= setupInsertLine)
                    lineNumber += setupLineOffset;

                bool prependNewline = importsVisitor.hasPackagesLocal() && config.completion.imports.separateGroupsWithLine &&
                                      importsVisitor.shouldPrependNewline(lineNumber);

                // 1. Suggest the package itself (e.g., GameTile)
                if (!importsVisitor.containsRequire(depName))
                {
                    items.emplace_back(createRotrieverPackageSuggestion(depName, packagesLocalName, lineNumber, prependNewline, setupEdits));
                }

                // 2. Suggest the package's exports (e.g., GameTileView, AppGameTile)
                // Look up the dependency package in discovered packages to get its exports
                for (const auto& [pkgRoot, pkg] : workspaceFolder->getRotrieverPackages())
                {
                    // Check if this package matches the dependency
                    if (pkgRoot.fsPath() == dep.resolvedPath.fsPath())
                    {
                        // Check if the package itself is already imported (we'll reuse it instead of require)
                        bool packageAlreadyImported = importsVisitor.containsRequire(depName);

                        // If the package is already imported, insert new lines AFTER the package import
                        // Otherwise, use the computed line number
                        size_t exportLineNumber = lineNumber;
                        if (packageAlreadyImported)
                        {
                            if (auto pkgLine = importsVisitor.getRequireLine(depName))
                                exportLineNumber = std::max(lineNumber, *pkgLine + 1);
                        }

                        // Suggest value exports (functions, tables, etc.)
                        for (const auto& exportPath : pkg.exports)
                        {
                            // For nested exports like "Events.gamePlayIntent", check if the local name is imported
                            std::string localName = exportPath;
                            if (auto lastDot = exportPath.rfind('.'); lastDot != std::string::npos)
                                localName = exportPath.substr(lastDot + 1);

                            // Skip if already imported
                            if (importsVisitor.containsRequire(localName))
                                continue;

                            items.emplace_back(createRotrieverExportSuggestion(
                                exportPath, depName, packagesLocalName, exportLineNumber, prependNewline, packageAlreadyImported, setupEdits));
                        }

                        // Suggest type exports (from "export type" statements)
                        for (const auto& typePath : pkg.typeExports)
                        {
                            // For nested types, get the local name
                            std::string localName = typePath;
                            if (auto lastDot = typePath.rfind('.'); lastDot != std::string::npos)
                                localName = typePath.substr(lastDot + 1);

                            // Skip if already imported (check both requires and type names)
                            // For now, we just check containsRequire which might miss type imports
                            // TODO: Add proper type import tracking
                            if (importsVisitor.containsRequire(localName))
                                continue;

                            items.emplace_back(createRotrieverTypeExportSuggestion(
                                typePath, depName, packagesLocalName, exportLineNumber, prependNewline, packageAlreadyImported, setupEdits));
                        }
                        break;
                    }
                }
            }
        }
    }
}
