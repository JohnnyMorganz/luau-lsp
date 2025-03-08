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
    std::shared_ptr<Client> client;
    std::unique_ptr<LSPPlatform> platform;
    std::string name;
    lsp::DocumentUri rootUri;
    WorkspaceFileResolver fileResolver;
    Luau::Frontend frontend;
    bool isConfigured = false;
    std::optional<nlohmann::json> definitionsFileMetadata;

public:
    WorkspaceFolder(const std::shared_ptr<Client>& client, std::string name, const lsp::DocumentUri& uri, std::optional<Luau::Config> defaultConfig)
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
        fileResolver.client = std::static_pointer_cast<BaseClient>(client);
        fileResolver.rootUri = uri;
    }

    // Sets up the workspace folder after receiving configuration information
    void setupWithConfiguration(const ClientConfiguration& configuration);

    void openTextDocument(const lsp::DocumentUri& uri, const lsp::DidOpenTextDocumentParams& params);
    void updateTextDocument(
        const lsp::DocumentUri& uri, const lsp::DidChangeTextDocumentParams& params, std::vector<Luau::ModuleName>* markedDirty = nullptr);
    void closeTextDocument(const lsp::DocumentUri& uri);

    void onDidChangeWatchedFiles(const lsp::FileEvent& change);

    /// Whether the file has been marked as ignored by any of the ignored lists in the configuration
    bool isIgnoredFile(const std::filesystem::path& path, const std::optional<ClientConfiguration>& givenConfig = std::nullopt);
    /// Whether the file has been marked as ignored for auto-importing
    bool isIgnoredFileForAutoImports(const std::filesystem::path& path, const std::optional<ClientConfiguration>& givenConfig = std::nullopt);
    /// Whether the file has been specified in the configuration as a definitions file
    bool isDefinitionFile(const std::filesystem::path& path, const std::optional<ClientConfiguration>& givenConfig = std::nullopt);

    lsp::DocumentDiagnosticReport documentDiagnostics(const lsp::DocumentDiagnosticParams& params);
    lsp::WorkspaceDiagnosticReport workspaceDiagnostics(const lsp::WorkspaceDiagnosticParams& params);
    void recomputeDiagnostics(const ClientConfiguration& config);
    void pushDiagnostics(const lsp::DocumentUri& uri, const size_t version);

    void clearDiagnosticsForFile(const lsp::DocumentUri& uri);

    void indexFiles(const ClientConfiguration& config);

    Luau::CheckResult checkSimple(const Luau::ModuleName& moduleName);
    void checkStrict(const Luau::ModuleName& moduleName, bool forAutocomplete = true);
    // TODO: Clip once new type solver is live
    const Luau::ModulePtr getModule(const Luau::ModuleName& moduleName, bool forAutocomplete = false) const;

private:
    void registerTypes(const std::vector<std::string>& disabledGlobals);
    void endAutocompletion(const lsp::CompletionParams& params);
    void suggestImports(const Luau::ModuleName& moduleName, const Luau::Position& position, const ClientConfiguration& config,
        const TextDocument& textDocument, std::vector<lsp::CompletionItem>& result, bool completingTypeReferencePrefix = true);
    lsp::WorkspaceEdit computeOrganiseRequiresEdit(const lsp::DocumentUri& uri);
    std::vector<Luau::ModuleName> findReverseDependencies(const Luau::ModuleName& moduleName);
    std::vector<Uri> findFilesForWorkspaceDiagnostics(const std::filesystem::path& rootPath, const ClientConfiguration& config);

public:
    std::vector<std::string> getComments(const Luau::ModuleName& moduleName, const Luau::Location& node);
    std::optional<std::string> getDocumentationForType(const Luau::TypeId ty);
    std::optional<std::string> getDocumentationForAstNode(const Luau::ModuleName& moduleName, const Luau::AstNode* node, const Luau::ScopePtr scope);
    std::optional<std::string> getDocumentationForAutocompleteEntry(const std::string& name, const Luau::AutocompleteEntry& entry,
        const std::vector<Luau::AstNode*>& ancestry, const Luau::ModulePtr& localModule);
    std::vector<Reference> findAllTableReferences(const Luau::TypeId ty, std::optional<Luau::Name> property = std::nullopt);
    std::vector<Reference> findAllFunctionReferences(const Luau::TypeId ty);
    std::vector<Reference> findAllTypeReferences(const Luau::ModuleName& moduleName, const Luau::Name& typeName);

    std::vector<lsp::CompletionItem> completion(const lsp::CompletionParams& params);

    std::vector<lsp::DocumentLink> documentLink(const lsp::DocumentLinkParams& params);
    lsp::DocumentColorResult documentColor(const lsp::DocumentColorParams& params);
    lsp::ColorPresentationResult colorPresentation(const lsp::ColorPresentationParams& params);
    lsp::CodeActionResult codeAction(const lsp::CodeActionParams& params);

    std::optional<lsp::Hover> hover(const lsp::HoverParams& params);

    std::optional<lsp::SignatureHelp> signatureHelp(const lsp::SignatureHelpParams& params);

    lsp::DefinitionResult gotoDefinition(const lsp::DefinitionParams& params);

    std::optional<lsp::Location> gotoTypeDefinition(const lsp::TypeDefinitionParams& params);

    lsp::ReferenceResult references(const lsp::ReferenceParams& params);
    lsp::RenameResult rename(const lsp::RenameParams& params);
    lsp::InlayHintResult inlayHint(const lsp::InlayHintParams& params);
    std::vector<lsp::FoldingRange> foldingRange(const lsp::FoldingRangeParams& params);

    std::vector<lsp::CallHierarchyItem> prepareCallHierarchy(const lsp::CallHierarchyPrepareParams& params);
    std::vector<lsp::CallHierarchyIncomingCall> callHierarchyIncomingCalls(const lsp::CallHierarchyIncomingCallsParams& params);
    std::vector<lsp::CallHierarchyOutgoingCall> callHierarchyOutgoingCalls(const lsp::CallHierarchyOutgoingCallsParams& params);

    std::optional<std::vector<lsp::DocumentSymbol>> documentSymbol(const lsp::DocumentSymbolParams& params);
    std::optional<std::vector<lsp::WorkspaceSymbol>> workspaceSymbol(const lsp::WorkspaceSymbolParams& params);
    std::optional<lsp::SemanticTokens> semanticTokens(const lsp::SemanticTokensParams& params);

    lsp::BytecodeResult bytecode(const lsp::BytecodeParams& params);
    lsp::CompilerRemarksResult compilerRemarks(const lsp::CompilerRemarksParams& params);

    bool isNullWorkspace() const
    {
        return name == "$NULL_WORKSPACE";
    };
};
