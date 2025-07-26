#pragma once
#include <iostream>
#include <memory>
#include "Platform/LSPPlatform.hpp"
#include "Luau/Frontend.h"
#include "Luau/Autocomplete.h"
#include "Protocol/Structures.hpp"
#include "Protocol/LanguageFeatures.hpp"
#include "Protocol/SignatureHelp.hpp"
#include "Protocol/SemanticTokens.hpp"
#include "Protocol/Extensions.hpp"
#include "LSP/Client.hpp"
#include "LSP/WorkspaceFileResolver.hpp"
#include "LSP/LuauExt.hpp"

using LSPCancellationToken = std::shared_ptr<Luau::FrontendCancellationToken>;

struct Reference
{
    Luau::ModuleName moduleName;
    Luau::Location location;

    bool operator==(const Reference& other) const
    {
        return moduleName == other.moduleName && location == other.location;
    }
};

class WorkspaceFolder
{
public:
    Client* client;
    std::unique_ptr<LSPPlatform> platform;
    std::string name;
    lsp::DocumentUri rootUri;
    WorkspaceFileResolver fileResolver;
    Luau::Frontend frontend;
    std::optional<nlohmann::json> definitionsFileMetadata;

    /// Whether this workspace folder has received configuration data.
    /// We postpone all initial messages until configuration data is received from the client.
    bool hasConfiguration = false;

    /// Whether the first-time configuration (platform, global types) have been applied for this folder.
    /// First-time configuration is only applied once, and changes require a language server restart
    bool appliedFirstTimeConfiguration = false;

    /// Whether this workspace folder has completed an initial set up process.
    /// Workspaces are initialized lazily on demand.
    /// When a new request comes in and the workspace is not ready, we will prepare it then.
    bool isReady = false;

public:
    WorkspaceFolder(Client* client, std::string name, const lsp::DocumentUri& uri, std::optional<Luau::Config> defaultConfig)
        : client(client)
        , name(std::move(name))
        , rootUri(uri)
        , fileResolver(defaultConfig ? WorkspaceFileResolver(*defaultConfig) : WorkspaceFileResolver())
        // TODO: we don't really need to retainFullTypeGraphs by default
        // but it seems that the option specified here is the one used
        // when calling Luau::autocomplete
        , frontend(Luau::Frontend(
              &fileResolver, &fileResolver, {/* retainFullTypeGraphs: */ true, /* forAutocomplete: */ false, /* runLintChecks: */ true}))
    {
        fileResolver.client = client;
        fileResolver.rootUri = uri;
    }

    /// Initializes the workspace on demand
    void lazyInitialize();

    // Sets up the workspace folder after receiving configuration information
    void setupWithConfiguration(const ClientConfiguration& configuration);

    void openTextDocument(const lsp::DocumentUri& uri, const lsp::DidOpenTextDocumentParams& params);
    void updateTextDocument(const lsp::DocumentUri& uri, const lsp::DidChangeTextDocumentParams& params);
    void onDidSaveTextDocument(const lsp::DocumentUri& uri, const lsp::DidSaveTextDocumentParams& params);
    void closeTextDocument(const lsp::DocumentUri& uri);

    void onDidChangeWatchedFiles(const std::vector<lsp::FileEvent>& changes);

    /// Whether the file has been marked as ignored by any of the ignored lists in the configuration
    bool isIgnoredFile(const Uri& uri, const std::optional<ClientConfiguration>& givenConfig = std::nullopt) const;
    /// Whether the file has been marked as ignored for auto-importing
    bool isIgnoredFileForAutoImports(const Uri& path, const std::optional<ClientConfiguration>& givenConfig = std::nullopt) const;
    /// Whether the file has been specified in the configuration as a definitions file
    bool isDefinitionFile(const Uri& path, const std::optional<ClientConfiguration>& givenConfig = std::nullopt) const;

    lsp::DocumentDiagnosticReport documentDiagnostics(
        const lsp::DocumentDiagnosticParams& params, const LSPCancellationToken& cancellationToken, bool allowUnmanagedFiles = false);
    lsp::WorkspaceDiagnosticReport workspaceDiagnostics(const lsp::WorkspaceDiagnosticParams& params);
    void recomputeDiagnostics(const ClientConfiguration& config);
    void pushDiagnostics(const lsp::DocumentUri& uri, const size_t version);

    void clearDiagnosticsForFiles(const std::vector<lsp::DocumentUri>& uri) const;
    void clearDiagnosticsForFile(const lsp::DocumentUri& uri);

    void indexFiles(const ClientConfiguration& config);

    Luau::CheckResult checkSimple(const Luau::ModuleName& moduleName, const LSPCancellationToken& cancellationToken);
    Luau::CheckResult checkStrict(const Luau::ModuleName& moduleName, const LSPCancellationToken& cancellationToken, bool forAutocomplete = true);
    // TODO: Clip once new type solver is live
    const Luau::ModulePtr getModule(const Luau::ModuleName& moduleName, bool forAutocomplete = false) const;

private:
    void registerTypes(const std::vector<std::string>& disabledGlobals);
    void endAutocompletion(const lsp::CompletionParams& params);
    void suggestImports(const Luau::ModuleName& moduleName, const Luau::Position& position, const ClientConfiguration& config,
        const TextDocument& textDocument, std::vector<lsp::CompletionItem>& result, bool completingTypeReferencePrefix = true);
    lsp::WorkspaceEdit computeOrganiseRequiresEdit(const lsp::DocumentUri& uri);
    std::vector<Luau::ModuleName> findReverseDependencies(const Luau::ModuleName& moduleName);
    std::vector<Uri> findFilesForWorkspaceDiagnostics(const std::string& rootPath, const ClientConfiguration& config);

public:
    std::vector<std::string> getComments(const Luau::ModuleName& moduleName, const Luau::Location& node);
    std::optional<std::string> getDocumentationForType(const Luau::TypeId ty);
    std::optional<std::string> getDocumentationForAstNode(const Luau::ModuleName& moduleName, const Luau::AstNode* node, const Luau::ScopePtr scope);
    std::optional<std::string> getDocumentationForAutocompleteEntry(const std::string& name, const Luau::AutocompleteEntry& entry,
        const std::vector<Luau::AstNode*>& ancestry, const Luau::ModulePtr& localModule);
    std::vector<Reference> findAllTableReferences(
        const Luau::TypeId ty, const LSPCancellationToken& cancellationToken, std::optional<Luau::Name> property = std::nullopt);
    std::vector<Reference> findAllFunctionReferences(const Luau::TypeId ty, const LSPCancellationToken& cancellationToken);
    std::vector<Reference> findAllTypeReferences(
        const Luau::ModuleName& moduleName, const Luau::Name& typeName, const LSPCancellationToken& cancellationToken);

    std::vector<lsp::CompletionItem> completion(const lsp::CompletionParams& params, const LSPCancellationToken& cancellationToken);

    std::vector<lsp::DocumentLink> documentLink(const lsp::DocumentLinkParams& params);
    lsp::DocumentColorResult documentColor(const lsp::DocumentColorParams& params);
    lsp::ColorPresentationResult colorPresentation(const lsp::ColorPresentationParams& params);
    lsp::CodeActionResult codeAction(const lsp::CodeActionParams& params);

    std::optional<lsp::Hover> hover(const lsp::HoverParams& params, const LSPCancellationToken& cancellationToken);

    std::optional<lsp::SignatureHelp> signatureHelp(const lsp::SignatureHelpParams& params, const LSPCancellationToken& cancellationToken);

    lsp::DefinitionResult gotoDefinition(const lsp::DefinitionParams& params, const LSPCancellationToken& cancellationToken);

    std::optional<lsp::Location> gotoTypeDefinition(const lsp::TypeDefinitionParams& params, const LSPCancellationToken& cancellationToken);

    lsp::ReferenceResult references(const lsp::ReferenceParams& params, const LSPCancellationToken& cancellationToken);
    lsp::RenameResult rename(const lsp::RenameParams& params, const LSPCancellationToken& cancellationToken);
    lsp::InlayHintResult inlayHint(const lsp::InlayHintParams& params, const LSPCancellationToken& cancellationToken);
    std::vector<lsp::FoldingRange> foldingRange(const lsp::FoldingRangeParams& params);

    std::vector<lsp::CallHierarchyItem> prepareCallHierarchy(
        const lsp::CallHierarchyPrepareParams& params, const LSPCancellationToken& cancellationToken);
    std::vector<lsp::CallHierarchyIncomingCall> callHierarchyIncomingCalls(const lsp::CallHierarchyIncomingCallsParams& params);
    std::vector<lsp::CallHierarchyOutgoingCall> callHierarchyOutgoingCalls(const lsp::CallHierarchyOutgoingCallsParams& params);

    std::optional<std::vector<lsp::DocumentSymbol>> documentSymbol(const lsp::DocumentSymbolParams& params);
    std::optional<std::vector<lsp::WorkspaceSymbol>> workspaceSymbol(const lsp::WorkspaceSymbolParams& params);
    std::optional<lsp::SemanticTokens> semanticTokens(const lsp::SemanticTokensParams& params, const LSPCancellationToken& cancellationToken);

    lsp::BytecodeResult bytecode(const lsp::BytecodeParams& params);
    lsp::CompilerRemarksResult compilerRemarks(const lsp::CompilerRemarksParams& params);

    bool isNullWorkspace() const
    {
        return name == "$NULL_WORKSPACE";
    };
};

void throwIfCancelled(const LSPCancellationToken& cancellationToken);
