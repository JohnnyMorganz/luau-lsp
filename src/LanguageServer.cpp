#include "LSP/LanguageServer.hpp"
#include "Flags.hpp"
#include "Luau/TimeTrace.h"

#include <string>
#include <variant>
#include <exception>
#include <algorithm>

#include "LSP/Uri.hpp"
#include "LSP/DocumentationParser.hpp"
#include "LSP/Diagnostics.hpp"

#ifdef LSP_BUILD_WITH_SENTRY
// sentry.h pulls in <windows.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#endif
#define SENTRY_BUILD_STATIC 1
#include <sentry.h>
#endif

#define ASSERT_PARAMS(params, method) \
    if (!params) \
        throw json_rpc::JsonRpcException(lsp::ErrorCode::InvalidParams, "params not provided for " method);

LanguageServer::LanguageServer(Client* aClient, std::optional<Luau::Config> aDefaultConfig)
    : client(aClient)
    , defaultConfig(std::move(aDefaultConfig))
    , nullWorkspace(std::make_shared<WorkspaceFolder>(client, "$NULL_WORKSPACE", Uri(), defaultConfig))
{
    messageProcessorThread = std::thread(
        [this]
        {
            while (auto message = popMessage())
                handleMessage(*message);
        });
}

/// Finds the workspace which the file belongs to.
/// If no workspace is found, the file is attached to the null workspace
WorkspaceFolderPtr LanguageServer::findWorkspace(const lsp::DocumentUri& file, bool shouldInitialize)
{
    if (file == nullWorkspace->rootUri)
    {
        if (shouldInitialize)
            nullWorkspace->lazyInitialize();
        return nullWorkspace;
    }

    WorkspaceFolderPtr bestWorkspace = nullptr;
    size_t length = 0;
    auto checkStr = file.toString();

    for (auto& workspace : workspaceFolders)
    {
        if (file == workspace->rootUri)
            return workspace;

        // Check if the root uri is a prefix of the file
        auto prefixStr = workspace->rootUri.toString();
        auto size = prefixStr.size();
        if (size < length)
            continue;

        if (checkStr.compare(0, size, prefixStr) == 0)
        {
            bestWorkspace = workspace;
            length = size;
        }
    }

    if (bestWorkspace)
    {
        if (shouldInitialize)
            bestWorkspace->lazyInitialize();
        return bestWorkspace;
    }

    client->sendTrace("cannot find workspace for " + file.toString());
    if (shouldInitialize)
        nullWorkspace->lazyInitialize();
    return nullWorkspace;
}

lsp::ServerCapabilities LanguageServer::getServerCapabilities()
{
    lsp::ServerCapabilities capabilities;
    // Text Document Sync
    lsp::TextDocumentSyncOptions textDocumentSyncOptions;
    textDocumentSyncOptions.openClose = true;
    textDocumentSyncOptions.change = lsp::TextDocumentSyncKind::Incremental;
    textDocumentSyncOptions.save = {/* includeText= */ false};
    capabilities.textDocumentSync = textDocumentSyncOptions;
    // Completion
    std::vector<std::string> completionTriggerCharacters{".", ":", "'", "\"", "/", "\n"}; // \n is used to trigger end completion
    lsp::CompletionOptions::CompletionItem completionItem{/* labelDetailsSupport: */ true};
    capabilities.completionProvider = {completionTriggerCharacters, std::nullopt, /* resolveProvider: */ false, completionItem};
    // Hover Provider
    capabilities.hoverProvider = true;
    // Signature Help
    std::vector<std::string> signatureHelpTriggerCharacters{"(", ","};
    capabilities.signatureHelpProvider = {signatureHelpTriggerCharacters};
    // Go To Declaration Provider
    capabilities.declarationProvider = false; // TODO: does this apply to Luau?
    // Go To Definition Provider
    capabilities.definitionProvider = true;
    // Go To Type Definition Provider
    capabilities.typeDefinitionProvider = true;
    // Go To Implementation Provider
    capabilities.implementationProvider = false; // TODO: does this apply to Luau?
    // Find References Provider
    capabilities.referencesProvider = true;
    // Document Symbol Provider
    capabilities.documentSymbolProvider = true;
    // Color Provider
    capabilities.colorProvider = true;
    // Document Link Provider
    capabilities.documentLinkProvider = {false};
    // Code Action Provider
    capabilities.codeActionProvider = {std::vector<lsp::CodeActionKind>{lsp::CodeActionKind::SourceOrganizeImports}, /* resolveProvider: */ false};
    // Rename Provider
    capabilities.renameProvider = true;
    // Folding Range Provider
    capabilities.foldingRangeProvider = true;
    // Inlay Hint Provider
    capabilities.inlayHintProvider = true;
    // Diagnostics Provider
    capabilities.diagnosticProvider = {"luau", /* interFileDependencies: */ true, /* workspaceDiagnostics: */ true};
    // Workspace Symbols Provider
    capabilities.workspaceSymbolProvider = true;
    // Call Hierarchy Provider
    capabilities.callHierarchyProvider = true;
    // Semantic Tokens Provider
    capabilities.semanticTokensProvider = {
        {
            std::vector<lsp::SemanticTokenTypes>(std::begin(lsp::SemanticTokenTypesList), std::end(lsp::SemanticTokenTypesList)),
            std::vector<lsp::SemanticTokenModifiers>(std::begin(lsp::SemanticTokenModifiersList), std::end(lsp::SemanticTokenModifiersList)),
        },
        /* range: */ false,
        /* full: */ true,
    };
    // Workspaces
    lsp::WorkspaceFoldersServerCapabilities workspaceFolderCapabilities{true, false};
    capabilities.workspace = lsp::WorkspaceCapabilities{workspaceFolderCapabilities};
    return capabilities;
}

void LanguageServer::onRequest(const id_type& id, const std::string& method, std::optional<json> baseParams)
{
    LUAU_TIMETRACE_SCOPE("LanguageServer::onRequest", "LSP");
    LUAU_TIMETRACE_ARGUMENT("method", method.c_str());

    // Handle request
    // If a request has been sent before the server is initialized, we should error
    if (!isInitialized && method != "initialize")
        throw JsonRpcException(lsp::ErrorCode::ServerNotInitialized, "server not initialized");
    // If we received a request after a shutdown, then we error with InvalidRequest
    if (shutdownRequested)
        throw JsonRpcException(lsp::ErrorCode::InvalidRequest, "server is shutting down");

    std::shared_ptr<Luau::FrontendCancellationToken> cancellationToken;
    if (const auto& it = cancellationTokens.find(id); it != cancellationTokens.end())
        cancellationToken = it->second;

    if (!cancellationToken)
    {
        LUAU_ASSERT(!"Have a request with no cancellation token");
        cancellationToken = std::make_shared<Luau::FrontendCancellationToken>();
    }

    if (cancellationToken->requested())
        throw JsonRpcException(lsp::ErrorCode::RequestCancelled, "request cancelled by client");

    Response response;

    if (method == "initialize")
    {
        response = onInitialize(JSON_REQUIRED_PARAMS(baseParams, "initialize"));
    }
    else if (method == "shutdown")
    {
        response = onShutdown(id);
    }
    else if (method == "textDocument/completion")
    {
        ASSERT_PARAMS(baseParams, "textDocument/completion")
        auto params = baseParams->get<lsp::CompletionParams>();
        auto workspace = findWorkspace(params.textDocument.uri);
        response = workspace->completion(params, cancellationToken);
    }
    else if (method == "textDocument/documentLink")
    {
        response = documentLink(JSON_REQUIRED_PARAMS(baseParams, "textDocument/documentLink"));
    }
    else if (method == "textDocument/hover")
    {
        ASSERT_PARAMS(baseParams, "textDocument/hover")
        auto params = baseParams->get<lsp::HoverParams>();
        auto workspace = findWorkspace(params.textDocument.uri);
        response = workspace->hover(params, cancellationToken);
    }
    else if (method == "textDocument/signatureHelp")
    {
        ASSERT_PARAMS(baseParams, "textDocument/signatureHelp")
        auto params = baseParams->get<lsp::SignatureHelpParams>();
        auto workspace = findWorkspace(params.textDocument.uri);
        response = workspace->signatureHelp(params, cancellationToken);
    }
    else if (method == "textDocument/definition")
    {
        ASSERT_PARAMS(baseParams, "textDocument/definition")
        auto params = baseParams->get<lsp::DefinitionParams>();
        auto workspace = findWorkspace(params.textDocument.uri);
        response = workspace->gotoDefinition(params, cancellationToken);
    }
    else if (method == "textDocument/typeDefinition")
    {
        ASSERT_PARAMS(baseParams, "textDocument/typeDefinition")
        auto params = baseParams->get<lsp::TypeDefinitionParams>();
        auto workspace = findWorkspace(params.textDocument.uri);
        response = workspace->gotoTypeDefinition(params, cancellationToken);
    }
    else if (method == "textDocument/references")
    {
        ASSERT_PARAMS(baseParams, "textDocument/references")
        auto params = baseParams->get<lsp::ReferenceParams>();
        auto workspace = findWorkspace(params.textDocument.uri);
        response = workspace->references(params, cancellationToken);
    }
    else if (method == "textDocument/rename")
    {
        ASSERT_PARAMS(baseParams, "textDocument/rename")
        auto params = baseParams->get<lsp::RenameParams>();
        auto workspace = findWorkspace(params.textDocument.uri);
        response = workspace->rename(params, cancellationToken);
    }
    else if (method == "textDocument/documentSymbol")
    {
        response = documentSymbol(JSON_REQUIRED_PARAMS(baseParams, "textDocument/documentSymbol"));
    }
    else if (method == "textDocument/codeAction")
    {
        response = codeAction(JSON_REQUIRED_PARAMS(baseParams, "textDocument/codeAction"));
    }
    // else if (method == "codeAction/resolve")
    // {
    //     response = codeActionResolve(JSON_REQUIRED_PARAMS(params, "codeAction/resolve"));
    // }
    else if (method == "textDocument/semanticTokens/full")
    {
        ASSERT_PARAMS(baseParams, "textDocument/semanticTokens/full")
        auto params = baseParams->get<lsp::SemanticTokensParams>();
        auto workspace = findWorkspace(params.textDocument.uri);
        response = workspace->semanticTokens(params, cancellationToken);
    }
    else if (method == "textDocument/inlayHint")
    {
        ASSERT_PARAMS(baseParams, "textDocument/inlayHint")
        auto params = baseParams->get<lsp::InlayHintParams>();
        auto workspace = findWorkspace(params.textDocument.uri);
        response = workspace->inlayHint(params, cancellationToken);
    }
    else if (method == "textDocument/documentColor")
    {
        response = documentColor(JSON_REQUIRED_PARAMS(baseParams, "textDocument/documentColor"));
    }
    else if (method == "textDocument/colorPresentation")
    {
        response = colorPresentation(JSON_REQUIRED_PARAMS(baseParams, "textDocument/colorPresentation"));
    }
    else if (method == "textDocument/prepareCallHierarchy")
    {
        ASSERT_PARAMS(baseParams, "textDocument/prepareCallHierarchy")
        auto params = baseParams->get<lsp::CallHierarchyPrepareParams>();
        auto workspace = findWorkspace(params.textDocument.uri);
        response = workspace->prepareCallHierarchy(params, cancellationToken);
    }
    else if (method == "callHierarchy/incomingCalls")
    {
        ASSERT_PARAMS(baseParams, "callHierarchy/incomingCalls")
        auto params = baseParams->get<lsp::CallHierarchyIncomingCallsParams>();
        auto workspace = findWorkspace(params.item.uri);
        response = workspace->callHierarchyIncomingCalls(params);
    }
    else if (method == "callHierarchy/outgoingCalls")
    {
        ASSERT_PARAMS(baseParams, "callHierarchy/outgoingCalls")
        auto params = baseParams->get<lsp::CallHierarchyOutgoingCallsParams>();
        auto workspace = findWorkspace(params.item.uri);
        response = workspace->callHierarchyOutgoingCalls(params);
    }
    else if (method == "textDocument/foldingRange")
    {
        ASSERT_PARAMS(baseParams, "textDocument/foldingRange")
        auto params = baseParams->get<lsp::FoldingRangeParams>();
        auto workspace = findWorkspace(params.textDocument.uri);
        response = workspace->foldingRange(params);
    }
    else if (method == "textDocument/diagnostic")
    {
        ASSERT_PARAMS(baseParams, "textDocument/diagnostic")
        auto params = baseParams->get<lsp::DocumentDiagnosticParams>();
        auto workspace = findWorkspace(params.textDocument.uri);
        response = workspace->documentDiagnostics(params, cancellationToken);
    }
    else if (method == "workspace/diagnostic")
    {
        // This request has partial request support.
        // If workspaceDiagnostic returns nothing, then we don't signal a response (as data will be sent as progress notifications)
        if (auto report = workspaceDiagnostic(JSON_REQUIRED_PARAMS(baseParams, "workspace/diagnostic")))
        {
            response = report;
        }
        else
        {
            client->workspaceDiagnosticsRequestId = id;
            return;
        }
    }
    else if (method == "workspace/symbol")
    {
        ASSERT_PARAMS(baseParams, "workspace/symbol")
        auto params = baseParams->get<lsp::WorkspaceSymbolParams>();

        std::vector<lsp::WorkspaceSymbol> result;
        for (auto& workspace : workspaceFolders)
        {
            auto report = workspace->workspaceSymbol(params);
            if (report)
                result.insert(result.end(), std::make_move_iterator(report->begin()), std::make_move_iterator(report->end()));
        }
        response = result;
    }
    else if (method == "luau-lsp/bytecode")
    {
        ASSERT_PARAMS(baseParams, "luau-lsp/bytecode")
        auto params = baseParams->get<lsp::BytecodeParams>();
        auto workspace = findWorkspace(params.textDocument.uri);
        response = workspace->bytecode(params);
    }

    else if (method == "luau-lsp/compilerRemarks")
    {
        ASSERT_PARAMS(baseParams, "luau-lsp/compilerRemarks")
        auto params = baseParams->get<lsp::CompilerRemarksParams>();
        auto workspace = findWorkspace(params.textDocument.uri);
        response = workspace->compilerRemarks(params);
    }
    else
    {
        throw JsonRpcException(lsp::ErrorCode::MethodNotFound, "method not found / supported: " + method);
    }

    client->sendResponse(id, response);
}

void LanguageServer::onNotification(const std::string& method, std::optional<json> params)
{
    LUAU_TIMETRACE_SCOPE("LanguageServer::onNotification", "LSP");
    LUAU_TIMETRACE_ARGUMENT("method", method.c_str());

    // Handle notification
    // If a notification is sent before the server is initialized or after a shutdown is requested (unless its exit), we should
    // drop it
    if ((!isInitialized || shutdownRequested) && method != "exit")
        return;

    if (method == "exit")
    {
        // Exit the process loop
        std::exit(shutdownRequested ? 0 : 1);
    }
    else if (method == "initialized")
    {
        onInitialized(JSON_REQUIRED_PARAMS(params, "initialized"));
    }
    else if (method == "$/setTrace")
    {
        client->setTrace(JSON_REQUIRED_PARAMS(params, "$/setTrace"));
    }
    else if (method == "$/cancelRequest")
    {
        // NO-OP: cancellation is handled in main loop
    }
    else if (method == "$/flushTimeTrace")
    {
#if defined(LUAU_ENABLE_TIME_TRACE)
        Luau::TimeTrace::getThreadContext().flushEvents();
#endif
    }
    else if (method == "textDocument/didOpen")
    {
        onDidOpenTextDocument(JSON_REQUIRED_PARAMS(params, "textDocument/didOpen"));
    }
    else if (method == "textDocument/didChange")
    {
        onDidChangeTextDocument(JSON_REQUIRED_PARAMS(params, "textDocument/didChange"));
    }
    else if (method == "textDocument/didSave")
    {
        onDidSaveTextDocument(JSON_REQUIRED_PARAMS(params, "textDocument/didSave"));
    }
    else if (method == "textDocument/didClose")
    {
        onDidCloseTextDocument(JSON_REQUIRED_PARAMS(params, "textDocument/didClose"));
    }
    else if (method == "workspace/didChangeConfiguration")
    {
        onDidChangeConfiguration(JSON_REQUIRED_PARAMS(params, "workspace/didChangeConfiguration"));
    }
    else if (method == "workspace/didChangeWorkspaceFolders")
    {
        onDidChangeWorkspaceFolders(JSON_REQUIRED_PARAMS(params, "workspace/didChangeWorkspaceFolders"));
    }
    else if (method == "workspace/didChangeWatchedFiles")
    {
        onDidChangeWatchedFiles(JSON_REQUIRED_PARAMS(params, "workspace/didChangeWatchedFiles"));
    }
    else
    {
        bool handled = false;
        for (auto& workspace : workspaceFolders)
        {
            if (workspace->platform && workspace->platform->handleNotification(method, params))
                handled = true;
        }

        if (!handled)
            client->sendLogMessage(lsp::MessageType::Warning, "unknown notification method: " + method);
    }
}

bool LanguageServer::allWorkspacesReceivedConfiguration() const
{
    for (auto& workspace : workspaceFolders)
    {
        if (!workspace->hasConfiguration)
            return false;
    }

    return true;
}

void LanguageServer::clearCancellationToken(const json_rpc::JsonRpcMessage& msg)
{
    if (!msg.is_request())
        return;

    std::unique_lock guard(messagesMutex);
    if (const auto& it = cancellationTokens.find(*msg.id); it != cancellationTokens.end())
        cancellationTokens.erase(it);
}

static bool notificationRequiresWorkspace(std::string_view method)
{
    return Luau::startsWith(method, "textDocument/") || Luau::startsWith(method, "workspace/");
}

void LanguageServer::handleMessage(const json_rpc::JsonRpcMessage& msg)
{
    try
    {
        if (msg.is_request())
        {
            if (isInitialized && !allWorkspacesReceivedConfiguration())
            {
                client->sendTrace("workspaces not configured, postponing message: " + msg.method.value());
                configPostponedMessages.emplace_back(msg);
                return;
            }

            client->sendTrace("Server now processing " + *msg.method + " (" + std::to_string(std::get<int>(msg.id.value())) + ")");
            onRequest(msg.id.value(), msg.method.value(), msg.params);
            clearCancellationToken(msg);
        }
        else if (msg.is_response())
        {
            client->sendTrace("Server now processing response to request " + std::to_string(std::get<int>(msg.id.value())));
            client->handleResponse(msg);

            // Receiving configuration is always at the end of a response. Now we can check to see if we can process any postponed messages
            if (!configPostponedMessages.empty() && allWorkspacesReceivedConfiguration())
            {
                client->sendTrace("workspaces configured, handling postponed messages");
                for (const auto& postponedMessage : configPostponedMessages)
                    handleMessage(postponedMessage);
                configPostponedMessages.clear();
                client->sendTrace("workspaces configured, handling postponed COMPLETED");
            }
        }
        else if (msg.is_notification())
        {
            if (isInitialized && !allWorkspacesReceivedConfiguration() && notificationRequiresWorkspace(msg.method.value()))
            {
                client->sendTrace("workspaces not configured, postponing notification: " + msg.method.value());
                configPostponedMessages.emplace_back(msg);
                return;
            }

            client->sendTrace("Server now processing notification: " + *msg.method);
            onNotification(msg.method.value(), msg.params);
        }
        else
        {
            throw JsonRpcException(lsp::ErrorCode::InvalidRequest, "invalid json-rpc message");
        }
    }
    catch (const JsonRpcException& e)
    {
        client->sendError(msg.id, e);
        clearCancellationToken(msg);
    }
    catch (const std::exception& e)
    {
#ifdef LSP_BUILD_WITH_SENTRY
        sentry_value_t event = sentry_value_new_event();
        sentry_value_t exc = sentry_value_new_exception("CaughtException", e.what());
        sentry_value_set_stacktrace(exc, NULL, 0);
        sentry_value_set_by_key(exc, "handled", sentry_value_new_bool(true));
        sentry_event_add_exception(event, exc);
        sentry_capture_event(event);
#endif
        client->sendError(msg.id, JsonRpcException(lsp::ErrorCode::InternalError, e.what()));
        clearCancellationToken(msg);
    }
}

std::optional<json_rpc::JsonRpcMessage> LanguageServer::popMessage()
{
    std::unique_lock guard(messagesMutex);

    messagesCv.wait(guard,
        [this]
        {
            return !messages.empty() || shutdownRequested;
        });

    if (shutdownRequested)
        return std::nullopt;

    auto message = messages.front();
    messages.pop();
    return message;
}

void LanguageServer::processInputLoop()
{
    std::string jsonString;
    while (std::cin)
    {
        if (client->readRawMessage(jsonString))
        {
            std::optional<id_type> id = std::nullopt;
            try
            {
                // Parse the input
                auto msg = json_rpc::parse(jsonString);
                id = msg.id;

                if (msg.is_request())
                    cancellationTokens[*msg.id] = std::make_shared<Luau::FrontendCancellationToken>();

                {
                    std::unique_lock guard(messagesMutex);
                    if (msg.is_notification() && msg.method == "$/cancelRequest")
                    {
                        ASSERT_PARAMS(msg.params, "$/cancelRequest")
                        auto params = msg.params->get<lsp::CancelParams>();
                        if (const auto& it = cancellationTokens.find(params.id); it != cancellationTokens.end())
                            it->second->cancel();

                        client->sendTrace("Processed cancellation for " + msg.params->dump());
                        continue;
                    }
                    messages.push(msg);
                }

                messagesCv.notify_one();
            }
            catch (const json::exception& e)
            {
                client->sendError(id, JsonRpcException(lsp::ErrorCode::ParseError, e.what()));
            }
        }
    }
}

bool LanguageServer::requestedShutdown()
{
    return shutdownRequested;
}

// Dispatch handlers
lsp::InitializeResult LanguageServer::onInitialize(const lsp::InitializeParams& params)
{
    LUAU_TIMETRACE_SCOPE("LanguageServer::onInitialize", "LSP");

    // Set provided settings
    client->sendTrace("client capabilities: " + json(params.capabilities).dump(), std::nullopt);
    client->capabilities = params.capabilities;
    client->traceMode = params.trace;

    // Set FFlags
    if (params.initializationOptions.has_value())
    {
        try
        {
            InitializationOptions options = params.initializationOptions.value();
            if (!options.fflags.empty())
            {
                registerFastFlags(
                    options.fflags,
                    [this](const std::string& message)
                    {
                        client->sendLogMessage(lsp::MessageType::Error, message);
                    },
                    [this](const std::string& message)
                    {
                        client->sendLogMessage(lsp::MessageType::Info, message);
                    });
            }
        }
        catch (const json::exception& err)
        {
            client->sendLogMessage(lsp::MessageType::Error, std::string("Failed to parse initialization options: ") + err.what());
        }
    }

    // Configure workspaces
    if (params.workspaceFolders.has_value())
    {
        for (auto& folder : params.workspaceFolders.value())
        {
            workspaceFolders.push_back(std::make_shared<WorkspaceFolder>(client, folder.name, folder.uri, defaultConfig));
        }
    }
    else if (params.rootUri.has_value())
    {
        workspaceFolders.push_back(std::make_shared<WorkspaceFolder>(client, "$ROOT", params.rootUri.value(), defaultConfig));
    }

    isInitialized = true;
    lsp::InitializeResult result;
    result.capabilities = getServerCapabilities();
    result.serverInfo = lsp::InitializeResult::ServerInfo{LSP_NAME, LSP_VERSION};

    // Position Encoding
    if (client->capabilities.general && client->capabilities.general->positionEncodings &&
        client->capabilities.general->positionEncodings->size() > 0)
    {
        auto& encodings = *client->capabilities.general->positionEncodings;
        if (encodings[0] == lsp::PositionEncodingKind::UTF8 || encodings[0] == lsp::PositionEncodingKind::UTF16)
            positionEncoding() = encodings[0];
    }
    result.capabilities.positionEncoding = positionEncoding();
    client->sendLogMessage(lsp::MessageType::Info, "negotiated position encoding: " + json(positionEncoding()).dump());

    client->sendTrace("server capabilities:" + json(result).dump(), std::nullopt);
    return result;
}

void LanguageServer::onInitialized([[maybe_unused]] const lsp::InitializedParams& params)
{
    LUAU_TIMETRACE_SCOPE("LanguageServer::onInitialized", "LSP");

    // Client received result of initialize
    client->sendLogMessage(lsp::MessageType::Info, "server initialized!");
    client->sendLogMessage(lsp::MessageType::Info, "trace level: " + json(client->traceMode).dump());

    // Handle configuration responses
    client->configChangedCallback = [&](const lsp::DocumentUri& workspaceUri, const ClientConfiguration& config, const ClientConfiguration* oldConfig)
    {
        auto workspace = findWorkspace(workspaceUri, /* shouldInitialize = */ false);

        workspace->hasConfiguration = true;
        if (!workspace->isReady)
            return;

        // Update the workspace setup with the new configuration
        workspace->setupWithConfiguration(config);

        // Refresh diagnostics
        workspace->recomputeDiagnostics(config);

        // Refresh inlay hint if changed
        if (!oldConfig || oldConfig->inlayHints != config.inlayHints)
            client->refreshInlayHints();
    };

    // Request configuration if the client supports it
    bool requestedConfiguration = false;
    if (client->capabilities.workspace && client->capabilities.workspace->configuration)
    {
        client->sendTrace("client supports configuration");
        requestedConfiguration = true;

        // Send off requests to get the configuration for each workspace
        client->sendTrace("config: requesting initial configuration for each workspace");
        std::vector<lsp::DocumentUri> items{nullWorkspace->rootUri};
        for (auto& workspace : workspaceFolders)
            items.emplace_back(workspace->rootUri);
        client->requestConfiguration(items);
    }

    // Dynamically register for configuration changed notifications
    if (client->capabilities.workspace && client->capabilities.workspace->didChangeConfiguration &&
        client->capabilities.workspace->didChangeConfiguration->dynamicRegistration)
    {
        client->sendTrace("client supports didChangedConfiguration, registering capability");
        client->registerCapability("didChangeConfigurationCapability", "workspace/didChangeConfiguration", nullptr);
    }

    // Dynamically register file watchers
    if (client->capabilities.workspace && client->capabilities.workspace->didChangeWatchedFiles &&
        client->capabilities.workspace->didChangeWatchedFiles->dynamicRegistration)
    {
        client->sendLogMessage(lsp::MessageType::Info, "registering didChangedWatchedFiles capability");

        std::vector<lsp::FileSystemWatcher> watchers{};
        watchers.push_back(lsp::FileSystemWatcher{"**/.luaurc"});
        watchers.push_back(lsp::FileSystemWatcher{"**/.robloxrc"});
        watchers.push_back(lsp::FileSystemWatcher{"**/*.{lua,luau}"});
        client->registerCapability(
            "didChangedWatchedFilesCapability", "workspace/didChangeWatchedFiles", lsp::DidChangeWatchedFilesRegistrationOptions{watchers});
    }
    else
    {
        client->sendLogMessage(lsp::MessageType::Warning,
            "client does not allow didChangeWatchedFiles registration - automatic updating on sourcemap/config changes will not be provided");
    }

    nullWorkspace->hasConfiguration = true;
    // Client does not support retrieving configuration information, so we just setup the workspaces with the default, global, configuration
    if (!requestedConfiguration)
    {
        for (auto& folder : workspaceFolders)
            folder->hasConfiguration = true;
    }
}

void LanguageServer::onDidOpenTextDocument(const lsp::DidOpenTextDocumentParams& params)
{
    // Start managing the file in-memory
    auto workspace = findWorkspace(params.textDocument.uri);
    workspace->openTextDocument(params.textDocument.uri, params);

    // Trigger diagnostics
    // By default, we rely on the pull based diagnostics model (based on documentDiagnostic)
    // however if a client doesn't yet support it, we push the diagnostics instead
    if (!usingPullDiagnostics(client->capabilities))
        workspace->pushDiagnostics(params.textDocument.uri, params.textDocument.version);
}

void LanguageServer::onDidChangeTextDocument(const lsp::DidChangeTextDocumentParams& params)
{
    // Update in-memory file with new contents
    auto workspace = findWorkspace(params.textDocument.uri);
    workspace->updateTextDocument(params.textDocument.uri, params);
}

void LanguageServer::onDidSaveTextDocument(const lsp::DidSaveTextDocumentParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    workspace->onDidSaveTextDocument(params.textDocument.uri, params);
}

void LanguageServer::onDidCloseTextDocument(const lsp::DidCloseTextDocumentParams& params)
{
    // Release managed in-memory file
    auto workspace = findWorkspace(params.textDocument.uri);
    workspace->closeTextDocument(params.textDocument.uri);
}

void LanguageServer::onDidChangeConfiguration(const lsp::DidChangeConfigurationParams& params)
{
    // We can't tell what workspace this is for, so we will have to request it from the client directly again
    if (client->capabilities.workspace && client->capabilities.workspace->configuration)
    {
        // Send off requests to get the configuration again for each workspace
        std::vector<lsp::DocumentUri> items{nullWorkspace->rootUri};
        for (auto& workspace : workspaceFolders)
            items.emplace_back(workspace->rootUri);
        client->requestConfiguration(items);
    }
    else
    {
        // We just have to assume these are the new global settings
        // We can't assume its formed correctly, so lets wrap it in a try-catch
        try
        {
            client->globalConfig = params.settings;
        }
        catch (const std::exception& e)
        {
            client->sendLogMessage(lsp::MessageType::Error, std::string("failed to refresh global configuration: ") + e.what());
        }
    }
}

void LanguageServer::onDidChangeWorkspaceFolders(const lsp::DidChangeWorkspaceFoldersParams& params)
{
    // Erase all old folders (this is O(n^2))
    for (auto it = workspaceFolders.begin(); it != workspaceFolders.end();)
    {
        auto name = (*it)->name;
        if (std::find_if(params.event.removed.begin(), params.event.removed.end(),
                [name](auto& w)
                {
                    return w.name == name;
                }) != std::end(params.event.removed))
        {
            it = workspaceFolders.erase(it);

            // Remove the configuration information for this folder
            client->removeConfiguration((*it)->rootUri);
        }
        else
        {
            ++it;
        }
    }

    // Add new folders
    std::vector<lsp::DocumentUri> configItems{};
    for (auto& folder : params.event.added)
    {
        workspaceFolders.emplace_back(std::make_shared<WorkspaceFolder>(client, folder.name, folder.uri, defaultConfig));
        configItems.emplace_back(folder.uri);
    }
    client->requestConfiguration(configItems);
}

void LanguageServer::onDidChangeWatchedFiles(const lsp::DidChangeWatchedFilesParams& params)
{
    Luau::DenseHashMap<WorkspaceFolderPtr, std::vector<lsp::FileEvent>> workspaceChanges{nullptr};

    for (const auto& change : params.changes)
    {
        auto workspace = findWorkspace(change.uri, /* shouldInitialize= */ false);
        if (!workspace->isReady)
            continue;

        if (!workspaceChanges.find(workspace))
            workspaceChanges[workspace] = {};
        workspaceChanges[workspace].push_back(change);
    }

    for (const auto& [workspace, changes] : workspaceChanges)
        workspace->onDidChangeWatchedFiles(changes);
}

Response LanguageServer::onShutdown([[maybe_unused]] const id_type& id)
{
    {
        std::unique_lock lock(messagesMutex);
        shutdownRequested = true;
    }

    messagesCv.notify_all();
    return nullptr;
}
