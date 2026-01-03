#include "LSP/Workspace.hpp"

#include <memory>

#include "LSP/Diagnostics.hpp"
#include "Platform/LSPPlatform.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "Plugin/PluginManager.hpp"
#include "glob/match.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/NotNull.h"
#include "Luau/TimeTrace.h"
#include "LuauFileUtils.hpp"

LUAU_FASTFLAG(LuauSolverV2)

void throwIfCancelled(const LSPCancellationToken& cancellationToken)
{
    if (cancellationToken && cancellationToken->requested())
        throw RequestCancelledException();
}

const Luau::ModulePtr WorkspaceFolder::getModule(const Luau::ModuleName& moduleName, bool forAutocomplete) const
{
    if (FFlag::LuauSolverV2 || !forAutocomplete)
        return frontend.moduleResolver.getModule(moduleName);
    else
        return frontend.moduleResolverForAutocomplete.getModule(moduleName);
}

void WorkspaceFolder::openTextDocument(const lsp::DocumentUri& uri, const lsp::DidOpenTextDocumentParams& params)
{
    LUAU_ASSERT(isReady);
    fileResolver.managedFiles.emplace(
        std::make_pair(uri, TextDocument(uri, params.textDocument.languageId, params.textDocument.version, params.textDocument.text)));

    // Mark the file as dirty as we don't know what changes were made to it
    auto moduleName = fileResolver.getModuleName(uri);
    frontend.markDirty(moduleName);
}

static bool isWorkspaceDiagnosticsEnabled(const Client* client, const ClientConfiguration& config)
{
    return client->workspaceDiagnosticsToken && config.diagnostics.workspace;
}

void WorkspaceFolder::updateTextDocument(const lsp::DocumentUri& uri, const lsp::DidChangeTextDocumentParams& params)
{
    LUAU_ASSERT(isReady);

    if (fileResolver.managedFiles.find(uri) == fileResolver.managedFiles.end())
    {
        client->sendLogMessage(lsp::MessageType::Error, "Text Document not loaded locally: " + uri.toString());
        return;
    }
    auto& textDocument = fileResolver.managedFiles.at(uri);
    textDocument.update(params.contentChanges, params.textDocument.version);

    // Invalidate plugin cache for this document - forces re-transformation on next access
    fileResolver.invalidatePluginDocument(uri);

    // Keep a vector of reverse dependencies marked dirty to extend diagnostics for them
    std::vector<Luau::ModuleName> markedDirty{};

    // Mark the module dirty for the typechecker
    frontend.markDirty(fileResolver.getModuleName(uri), &markedDirty);

    // In pull based diagnostics module, documentDiagnostics will update the necessary files
    // But if we are still using push-based diagnostics, we need to send updates
    auto config = client->getConfiguration(rootUri);
    if (!usingPullDiagnostics(client->capabilities))
    {
        // Convert the diagnostics report into a series of diagnostics published for each relevant file
        auto diagnostics = documentDiagnostics(lsp::DocumentDiagnosticParams{{uri}}, /* cancellationToken= */ nullptr);
        client->publishDiagnostics(lsp::PublishDiagnosticsParams{uri, params.textDocument.version, diagnostics.items});

        // Compute diagnostics for reverse dependencies
        // TODO: should we put this inside documentDiagnostics so it works in the pull based model as well? (its a reverse BFS which is expensive)
        if (config.diagnostics.includeDependents)
        {
            for (auto& moduleName : markedDirty)
            {
                auto dirtyUri = fileResolver.getUri(moduleName);
                if (dirtyUri != uri && diagnostics.relatedDocuments.find(dirtyUri) == diagnostics.relatedDocuments.end() &&
                    !isIgnoredFile(dirtyUri, config))
                {
                    auto dependencyDiags = documentDiagnostics(
                        lsp::DocumentDiagnosticParams{{dirtyUri}}, /* cancellationToken=*/nullptr, /* allowUnmanagedFiles= */ true);
                    client->publishDiagnostics(lsp::PublishDiagnosticsParams{dirtyUri, std::nullopt, dependencyDiags.items});
                }
            }
        }
    }
}

void WorkspaceFolder::onDidSaveTextDocument(const lsp::DocumentUri& uri, const lsp::DidSaveTextDocumentParams& params)
{
    LUAU_ASSERT(isReady);

    auto config = client->getConfiguration(rootUri);
    if (isWorkspaceDiagnosticsEnabled(client, config))
    {
        Luau::DenseHashSet<Luau::ModuleName> dependents{""};
        frontend.traverseDependents(fileResolver.getModuleName(uri),
            [&dependents](Luau::SourceNode& sourceNode)
            {
                if (dependents.contains(sourceNode.name))
                    return false;

                dependents.insert(sourceNode.name);
                return true;
            });

        lsp::WorkspaceDiagnosticReportPartialResult report;

        // Convert the diagnostics report into a series of diagnostics published for each relevant file
        auto diagnostics = documentDiagnostics(lsp::DocumentDiagnosticParams{{uri}}, /* cancellationToken= */ nullptr);

        lsp::WorkspaceDocumentDiagnosticReport mainDocumentReport;
        mainDocumentReport.uri = uri;
        mainDocumentReport.kind = diagnostics.kind;
        mainDocumentReport.items = diagnostics.items;
        mainDocumentReport.items = diagnostics.items;
        report.items.emplace_back(mainDocumentReport);

        for (auto& moduleName : dependents)
        {
            auto dirtyUri = fileResolver.getUri(moduleName);
            if (dirtyUri != uri && !isIgnoredFile(dirtyUri, config))
            {
                auto dependencyDiags =
                    documentDiagnostics(lsp::DocumentDiagnosticParams{{dirtyUri}}, /* cancellationToken= */ nullptr, /* allowUnmanagedFiles= */ true);

                lsp::WorkspaceDocumentDiagnosticReport documentReport;
                documentReport.uri = dirtyUri;
                documentReport.kind = dependencyDiags.kind;
                documentReport.items = dependencyDiags.items;
                documentReport.items = dependencyDiags.items;
                report.items.emplace_back(documentReport);
            }
        }

        client->sendProgress({*client->workspaceDiagnosticsToken, report});
    }
}

void WorkspaceFolder::closeTextDocument(const lsp::DocumentUri& uri)
{
    fileResolver.managedFiles.erase(uri);

    // Mark the module as dirty as we no longer track its changes
    auto config = client->getConfiguration(rootUri);
    auto moduleName = fileResolver.getModuleName(uri);
    frontend.markDirty(moduleName);

    // Refresh workspace diagnostics to clear diagnostics on ignored files
    if (!config.diagnostics.workspace || isIgnoredFile(uri))
        clearDiagnosticsForFile(uri);
}

void WorkspaceFolder::clearDiagnosticsForFiles(const std::vector<lsp::DocumentUri>& uris) const
{
    if (!client->capabilities.textDocument || !client->capabilities.textDocument->diagnostic)
    {
        for (const auto& uri : uris)
            client->publishDiagnostics(lsp::PublishDiagnosticsParams{uri, std::nullopt, {}});
    }
    else if (client->workspaceDiagnosticsToken)
    {
        std::vector<lsp::WorkspaceDocumentDiagnosticReport> reports;
        reports.reserve(uris.size());
        for (const auto& uri : uris)
        {
            lsp::WorkspaceDocumentDiagnosticReport report;
            report.uri = uri;
            report.kind = lsp::DocumentDiagnosticReportKind::Full;
            reports.push_back(report);
        }
        lsp::WorkspaceDiagnosticReportPartialResult report{reports};
        client->sendProgress({client->workspaceDiagnosticsToken.value(), report});
    }
    else
    {
        client->refreshWorkspaceDiagnostics();
    }
}

void WorkspaceFolder::clearDiagnosticsForFile(const lsp::DocumentUri& uri)
{
    clearDiagnosticsForFiles({uri});
}

static const char* kWatchedFilesProgressToken = "luau/onDidChangeWatchedFiles";

void WorkspaceFolder::onDidChangeWatchedFiles(const std::vector<lsp::FileEvent>& changes)
{
    client->sendTrace("workspace: processing " + std::to_string(changes.size()) + " watched files changes");

    client->createWorkDoneProgress(kWatchedFilesProgressToken);
    client->sendWorkDoneProgressBegin(kWatchedFilesProgressToken, "Luau: Processing " + std::to_string(changes.size()) + " file changes");

    auto config = client->getConfiguration(rootUri);

    std::vector<Luau::ModuleName> dirtyFiles;
    std::vector<Uri> deletedFiles;

    for (const auto& change : changes)
    {
        platform->onDidChangeWatchedFiles(change);

        if (change.uri.filename() == ".luaurc" || change.uri.filename() == ".robloxrc" || change.uri.filename() == ".config.luau")
        {
            client->sendLogMessage(lsp::MessageType::Info, "Acknowledge config changed for workspace " + name + ", clearing configuration cache");
            fileResolver.clearConfigCache();

            // Recompute diagnostics
            recomputeDiagnostics(config);
        }
        else if (change.uri.extension() == ".lua" || change.uri.extension() == ".luau")
        {
            // Notify if it was a definitions file
            if (isDefinitionFile(change.uri, config))
            {
                client->sendWindowMessage(
                    lsp::MessageType::Info, "Detected changes to global definitions files. Please reload your workspace for this to take effect");
                continue;
            }

            // Note: we should always mark as dirty, even if the file is ignored
            auto moduleName = fileResolver.getModuleName(change.uri);
            frontend.markDirty(moduleName, &dirtyFiles);

            if (change.type == lsp::FileChangeType::Deleted)
                deletedFiles.push_back(change.uri);
        }
    }

    // Parse require graph for files if indexing enable
    if (config.index.enabled && appliedFirstTimeConfiguration)
        frontend.parseModules(dirtyFiles);

    // Clear the diagnostics for files in case it was not managed
    clearDiagnosticsForFiles(deletedFiles);

    client->sendWorkDoneProgressEnd(kWatchedFilesProgressToken);
}

/// Whether the file has been marked as ignored by any of the ignored lists in the configuration
bool WorkspaceFolder::isIgnoredFile(const Uri& uri, const std::optional<ClientConfiguration>& givenConfig) const
{
    // We want to test globs against a relative path to workspace, since that's what makes most sense
    auto relativePathString = uri.lexicallyRelative(rootUri);
    auto config = givenConfig ? *givenConfig : client->getConfiguration(rootUri);
    std::vector<std::string> patterns = config.ignoreGlobs; // TODO: extend further?
    for (auto& pattern : patterns)
    {
        if (glob::gitignore_glob_match(relativePathString, pattern))
        {
            return true;
        }
    }
    return false;
}

bool WorkspaceFolder::isIgnoredFileForAutoImports(const Uri& uri, const std::optional<ClientConfiguration>& givenConfig) const
{
    // We want to test globs against a relative path to workspace, since that's what makes most sense
    auto relativePathString = uri.lexicallyRelative(rootUri);
    auto config = givenConfig ? *givenConfig : client->getConfiguration(rootUri);
    std::vector<std::string> patterns = config.completion.imports.ignoreGlobs;
    for (auto& pattern : patterns)
    {
        if (glob::gitignore_glob_match(relativePathString, pattern))
        {
            return true;
        }
    }
    return false;
}

bool WorkspaceFolder::isDefinitionFile(const Uri& path, const std::optional<ClientConfiguration>& givenConfig) const
{
    auto config = givenConfig ? *givenConfig : client->getConfiguration(rootUri);

    for (auto& [_, file] : config.types.definitionFiles)
    {
        if (rootUri.resolvePath(resolvePath(file)) == path)
        {
            return true;
        }
    }

    return false;
}

// Runs `Frontend::check` on the module and DISCARDS THE TYPE GRAPH.
// Uses the diagnostic type checker, so strictness and DM awareness is not enforced
// NOTE: do NOT use this if you later retrieve a ModulePtr (via frontend.moduleResolver.getModule). Instead use `checkStrict`
// NOTE: use `frontend.parse` if you do not care about typechecking
Luau::CheckResult WorkspaceFolder::checkSimple(const Luau::ModuleName& moduleName, const LSPCancellationToken& cancellationToken)
{
    try
    {
        Luau::FrontendOptions options{/* retainFullTypeGraphs: */ false, /* forAutocomplete: */ false, /* runLintChecks: */ true};
        options.cancellationToken = cancellationToken;
        return frontend.check(moduleName, options);
    }
    catch (Luau::InternalCompilerError& err)
    {
        // TODO: RecursionLimitException is leaking out of frontend.check
        // https://github.com/Roblox/luau/issues/975
        // Remove this try-catch block once the above issue is fixed
        client->sendLogMessage(lsp::MessageType::Warning, "Luau InternalCompilerError caught in " + moduleName + ": " + err.what());
        return Luau::CheckResult{};
    }
}

// Runs `Frontend::check` on the module whilst retaining the type graph.
// Uses the autocomplete typechecker to enforce strictness and DM awareness.
// NOTE: a disadvantage of the autocomplete typechecker is that it has a timeout restriction that
// can often be hit
Luau::CheckResult WorkspaceFolder::checkStrict(
    const Luau::ModuleName& moduleName, const LSPCancellationToken& cancellationToken, bool forAutocomplete)
{
    if (FFlag::LuauSolverV2)
        forAutocomplete = false;

    // HACK: note that a previous call to `Frontend::check(moduleName, { retainTypeGraphs: false })`
    // and then a call `Frontend::check(moduleName, { retainTypeGraphs: true })` will NOT actually
    // retain the type graph if the module is not marked dirty.
    // We do a manual check and dirty marking to fix this
    auto module = getModule(moduleName, forAutocomplete);
    if (module && module->internalTypes.types.empty()) // If we didn't retain type graphs, then the internalTypes arena is empty
        frontend.markDirty(moduleName);

    Luau::FrontendOptions options{/* retainFullTypeGraphs: */ true, forAutocomplete, /* runLintChecks: */ true};
    options.cancellationToken = cancellationToken;
    return frontend.check(moduleName, options);
}

static const char* kIndexProgressToken = "luau/indexFiles";

void WorkspaceFolder::indexFiles(const ClientConfiguration& config)
{
    LUAU_TIMETRACE_SCOPE("WorkspaceFolder::indexFiles", "LSP");
    if (!config.index.enabled)
        return;

    if (isNullWorkspace())
        return;

    client->sendTrace("workspace: indexing all files");
    client->createWorkDoneProgress(kIndexProgressToken);
    client->sendWorkDoneProgressBegin(kIndexProgressToken, "Luau: Indexing");

    std::vector<Luau::ModuleName> moduleNames;
    std::vector<std::string> directories{rootUri.fsPath()};

    auto luauConfig = fileResolver.readConfigRec(rootUri, limits);
    for (const auto& [aliasName, aliasInfo] : luauConfig.aliases)
    {
        auto uri = resolveAliasLocation(aliasInfo);
        if (!rootUri.isAncestorOf(uri))
        {
            if (uri.isDirectory())
                directories.emplace_back(uri.fsPath());
            else
                moduleNames.emplace_back(fileResolver.getModuleName(uri));
        }
    }

    bool sentMessage = false;
    for (const auto& directory : directories)
    {
        client->sendTrace("workspace: indexing files from '" + directory + "'");
        Luau::FileUtils::traverseDirectoryRecursive(directory,
            [&](auto& path)
            {
                if (moduleNames.size() >= config.index.maxFiles)
                {
                    if (!sentMessage)
                    {
                        client->sendWindowMessage(
                            lsp::MessageType::Warning, "The maximum workspace index limit (" + std::to_string(config.index.maxFiles) +
                                                           ") has been hit. This may cause some language features to only work partially "
                                                           "(Find All References, Rename). If necessary, consider increasing the limit");
                        sentMessage = true;
                    }
                    return;
                }

                auto uri = Uri::file(path);
                auto ext = uri.extension();
                if ((ext == ".lua" || ext == ".luau") && !isDefinitionFile(uri, config) && !isIgnoredFile(uri, config))
                {
                    auto moduleName = fileResolver.getModuleName(uri);
                    moduleNames.emplace_back(moduleName);
                }
            });
    }

    client->sendWorkDoneProgressReport(kIndexProgressToken, std::to_string(moduleNames.size()) + " files");

    frontend.clearStats();
    frontend.parseModules(moduleNames);

    client->sendLogMessage(lsp::MessageType::Info,
        "Indexed " + std::to_string(frontend.stats.files) + " files (" + std::to_string(frontend.stats.lines) +
            " lines)\n Time read: " + std::to_string(frontend.stats.timeRead) + "\n Time parse: " + std::to_string(frontend.stats.timeParse));

    client->sendWorkDoneProgressEnd(kIndexProgressToken, "Indexed " + std::to_string(moduleNames.size()) + " files");
    client->sendTrace("workspace: indexing all files COMPLETED");
}

static void clearDisabledGlobals(const Client* client, const Luau::GlobalTypes& globalTypes, const std::vector<std::string>& disabledGlobals)
{
    const auto targetScope = globalTypes.globalScope;
    for (const auto& disabledGlobal : disabledGlobals)
    {
        std::string library = disabledGlobal;
        std::optional<std::string> method = std::nullopt;

        if (const auto separator = disabledGlobal.find('.'); separator != std::string::npos)
        {
            library = disabledGlobal.substr(0, separator);
            method = disabledGlobal.substr(separator + 1);
        }

        const auto globalName = globalTypes.globalNames.names->get(library.c_str());
        if (globalName.value == nullptr)
        {
            client->sendLogMessage(lsp::MessageType::Warning, "disabling globals: skipping unknown global - " + disabledGlobal);
            continue;
        }

        if (auto binding = targetScope->bindings.find(globalName); binding != targetScope->bindings.end())
        {
            if (method)
            {
                const auto typeId = Luau::follow(binding->second.typeId);
                if (const auto ttv = Luau::getMutable<Luau::TableType>(typeId))
                {
                    if (contains(ttv->props, *method))
                    {
                        client->sendLogMessage(lsp::MessageType::Info, "disabling globals: erasing global - " + disabledGlobal);
                        ttv->props.erase(*method);
                    }
                    else
                        client->sendLogMessage(lsp::MessageType::Warning, "disabling globals: could not find method - " + disabledGlobal);
                }
                else if (const auto ctv = Luau::getMutable<Luau::ExternType>(typeId))
                {
                    if (contains(ctv->props, *method))
                    {
                        client->sendLogMessage(lsp::MessageType::Info, "disabling globals: erasing global - " + disabledGlobal);
                        ctv->props.erase(*method);
                    }
                    else
                        client->sendLogMessage(lsp::MessageType::Warning, "disabling globals: could not find method - " + disabledGlobal);
                }
                else
                {
                    client->sendLogMessage(lsp::MessageType::Warning,
                        "disabling globals: cannot clear method from global, only tables or classes are supported - " + disabledGlobal);
                }
            }
            else
            {
                client->sendLogMessage(lsp::MessageType::Info, "disabling globals: erasing global - " + disabledGlobal);
                targetScope->bindings.erase(globalName);
            }
        }
        else
        {
            client->sendLogMessage(lsp::MessageType::Warning, "disabling globals: skipping unknown global - " + disabledGlobal);
        }
    }
}

void WorkspaceFolder::registerTypes(const std::vector<std::string>& disabledGlobals)
{
    LUAU_TIMETRACE_SCOPE("WorkspaceFolder::initialize", "LSP");
    client->sendTrace("workspace initialization: registering Luau globals");
    Luau::registerBuiltinGlobals(frontend, frontend.globals);
    if (!FFlag::LuauSolverV2)
        Luau::registerBuiltinGlobals(frontend, frontend.globalsForAutocomplete);

    auto& tagRegisterGlobals = FFlag::LuauSolverV2 ? frontend.globals : frontend.globalsForAutocomplete;
    Luau::attachTag(Luau::getGlobalBinding(tagRegisterGlobals, "require"), "Require");

    if (client->definitionsFiles.empty())
        client->sendLogMessage(lsp::MessageType::Warning, "No definitions file provided by client");

    // For backwards compatibility, we need to keep an ordering where a definitions file for '@roblox' is always processed first
    std::vector<std::pair<std::string, std::string>> definitionsFilesToProcess{};
    definitionsFilesToProcess.reserve(client->definitionsFiles.size());
    if (auto it = client->definitionsFiles.find("@roblox"); it != client->definitionsFiles.end())
        definitionsFilesToProcess.emplace_back(*it);
    for (const auto& pair : client->definitionsFiles)
    {
        if (pair.first != "@roblox")
            definitionsFilesToProcess.emplace_back(pair);
    }

    for (const auto& [packageName, definitionsFile] : definitionsFilesToProcess)
    {
        auto resolvedFilePath = resolvePath(definitionsFile);
        client->sendLogMessage(lsp::MessageType::Info, "Loading definitions file: " + packageName + " - " + resolvedFilePath);

        auto definitionsContents = Luau::FileUtils::readFile(resolvedFilePath);
        if (!definitionsContents)
        {
            client->sendWindowMessage(
                lsp::MessageType::Error, "Failed to read definitions file " + resolvedFilePath + ". Extended types will not be provided");
            continue;
        }

        // Parse definitions file metadata
        client->sendTrace("workspace initialization: parsing definitions file metadata");
        auto metadata = types::parseDefinitionsFileMetadata(*definitionsContents);
        if (!definitionsFileMetadata)
            definitionsFileMetadata = metadata;
        client->sendTrace("workspace initialization: parsing definitions file metadata COMPLETED", json(definitionsFileMetadata).dump());

        client->sendTrace("workspace initialization: registering types definition");
        auto result = types::registerDefinitions(frontend, frontend.globals, packageName, *definitionsContents);
        if (!FFlag::LuauSolverV2)
            types::registerDefinitions(frontend, frontend.globalsForAutocomplete, packageName, *definitionsContents);
        client->sendTrace("workspace initialization: registering types definition COMPLETED");

        client->sendTrace("workspace: applying platform mutations on definitions");
        platform->mutateRegisteredDefinitions(frontend.globals, metadata);
        platform->mutateRegisteredDefinitions(frontend.globalsForAutocomplete, metadata);

        auto uri = Uri::file(resolvedFilePath);

        if (result.success)
        {
            // Clear any set diagnostics
            client->publishDiagnostics({uri, std::nullopt, {}});
            TextDocument textDocument(Uri::file(resolvedFilePath), "luau", 0, *definitionsContents);
            definitionsSourceModules.emplace(packageName, std::make_pair(textDocument, result.sourceModule));
        }
        else
        {
            client->sendWindowMessage(
                lsp::MessageType::Error, "Failed to read definitions file " + resolvedFilePath + ". Extended types will not be provided");

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

    if (!disabledGlobals.empty())
    {
        client->sendTrace("workspace initialization: removing disabled globals");
        clearDisabledGlobals(client, frontend.globals, disabledGlobals);
        if (!FFlag::LuauSolverV2)
            clearDisabledGlobals(client, frontend.globalsForAutocomplete, disabledGlobals);
        client->sendTrace("workspace initialization: removing disabled globals COMPLETED");
    }

    Luau::freeze(frontend.globals.globalTypes);
    if (!FFlag::LuauSolverV2)
        Luau::freeze(frontend.globalsForAutocomplete.globalTypes);
}

void WorkspaceFolder::lazyInitialize()
{
    if (isReady)
        return;

    LUAU_TIMETRACE_SCOPE("WorkspaceFolder::lazyInitialize", "LSP");

    if (isNullWorkspace())
    {
        client->sendTrace("initializing null workspace");
        setupWithConfiguration(client->globalConfig);
    }
    else
    {
        client->sendTrace("initializing workspace: " + rootUri.toString());
        auto config = client->getConfiguration(rootUri);
        setupWithConfiguration(config);
    }

    isReady = true;
}

void WorkspaceFolder::setupWithConfiguration(const ClientConfiguration& configuration)
{
    LUAU_TIMETRACE_SCOPE("WorkspaceFolder::setupWithConfiguration", "LSP");
    client->sendTrace("workspace: setting up with configuration");

    // Apply first-time configuration
    if (!appliedFirstTimeConfiguration)
    {
        appliedFirstTimeConfiguration = true;

        client->sendTrace("workspace: first time configuration, setting appropriate platform");
        platform = LSPPlatform::getPlatform(configuration, &fileResolver, this);
        fileResolver.platform = platform.get();
        fileResolver.requireSuggester = fileResolver.platform->getRequireSuggester();

        registerTypes(configuration.types.disabledGlobals);
    }

    client->sendTrace("workspace: apply platform-specific configuration");

    platform->setupWithConfiguration(configuration);

    // Configure plugins
    if (configuration.plugins.enabled && !configuration.plugins.paths.empty())
    {
        client->sendTrace("workspace: configuring plugins");

        // Always recreate the plugin manager to ensure clean state
        fileResolver.pluginManager = std::make_unique<Luau::LanguageServer::Plugin::PluginManager>(client, Luau::NotNull<WorkspaceFolder>{this});

        size_t loadedCount = fileResolver.pluginManager->configure(configuration.plugins.paths, configuration.plugins.timeoutMs);
        client->sendLogMessage(lsp::MessageType::Info, "Loaded " + std::to_string(loadedCount) + " of " +
            std::to_string(configuration.plugins.paths.size()) + " plugins");

        // Clear plugin document cache when plugins change
        fileResolver.clearPluginDocuments();
    }
    else if (fileResolver.pluginManager)
    {
        // Plugins disabled - clear plugin manager and caches
        fileResolver.pluginManager.reset();
        fileResolver.clearPluginDocuments();
    }

    if (configuration.index.enabled)
        indexFiles(configuration);

    client->sendTrace("workspace: setting up with configuration COMPLETED");
}
