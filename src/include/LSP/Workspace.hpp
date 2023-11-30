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
#include "LSP/Client.hpp"
#include "LSP/WorkspaceFileResolver.hpp"
#include "LSP/LuauExt.hpp"

/// Defining sort text levels assigned to completion items
/// Note that sort text is lexicographically
namespace SortText
{
static constexpr const char* PrioritisedSuggestion = "0";
static constexpr const char* TableProperties = "1";
static constexpr const char* CorrectTypeKind = "2";
static constexpr const char* CorrectFunctionResult = "3";
static constexpr const char* Default = "4";
static constexpr const char* WrongIndexType = "5";
static constexpr const char* MetatableIndex = "6";
static constexpr const char* AutoImports = "7";
static constexpr const char* AutoImportsAbsolute = "71";
static constexpr const char* Keywords = "8";
static constexpr const char* Deprioritized = "9";
} // namespace SortText

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
              &fileResolver, &fileResolver, {/* retainFullTypeGraphs: */ true, /* forAutocomplete: */ false, /* runLintChecks: */ false}))
    {
        fileResolver.client = std::static_pointer_cast<BaseClient>(client);
        fileResolver.rootUri = uri;
    }

    // Initialises the workspace folder
    void initialize();

    // Sets up the workspace folder after receiving configuration information
    void setupWithConfiguration(const ClientConfiguration& configuration);

    void openTextDocument(const lsp::DocumentUri& uri, const lsp::DidOpenTextDocumentParams& params);
    void updateTextDocument(
        const lsp::DocumentUri& uri, const lsp::DidChangeTextDocumentParams& params, std::vector<Luau::ModuleName>* markedDirty = nullptr);
    void closeTextDocument(const lsp::DocumentUri& uri);

    /// Whether the file has been marked as ignored by any of the ignored lists in the configuration
    bool isIgnoredFile(const std::filesystem::path& path, const std::optional<ClientConfiguration>& givenConfig = std::nullopt);
    /// Whether the file has been specified in the configuration as a definitions file
    bool isDefinitionFile(const std::filesystem::path& path, const std::optional<ClientConfiguration>& givenConfig = std::nullopt);

    lsp::DocumentDiagnosticReport documentDiagnostics(const lsp::DocumentDiagnosticParams& params);
    lsp::WorkspaceDiagnosticReport workspaceDiagnostics(const lsp::WorkspaceDiagnosticParams& params);

    void clearDiagnosticsForFile(const lsp::DocumentUri& uri);

    void indexFiles(const ClientConfiguration& config);

    Luau::CheckResult checkSimple(const Luau::ModuleName& moduleName, bool runLintChecks = false);
    void checkStrict(const Luau::ModuleName& moduleName, bool forAutocomplete = true);

private:
    void endAutocompletion(const lsp::CompletionParams& params);
    void suggestImports(const Luau::ModuleName& moduleName, const Luau::Position& position, const ClientConfiguration& config,
        const TextDocument& textDocument, std::vector<lsp::CompletionItem>& result, bool isType = true);
    lsp::WorkspaceEdit computeOrganiseRequiresEdit(const lsp::DocumentUri& uri);
    std::vector<Luau::ModuleName> findReverseDependencies(const Luau::ModuleName& moduleName);
    std::optional<lsp::CompletionItemKind> entryKind(const Luau::AutocompleteEntry& entry);

public:
    std::vector<std::string> getComments(const Luau::ModuleName& moduleName, const Luau::Location& node);
    std::optional<std::string> getDocumentationForType(const Luau::TypeId ty);
    std::optional<std::string> getDocumentationForAutocompleteEntry(
        const Luau::AutocompleteEntry& entry, const std::vector<Luau::AstNode*>& ancestry, const Luau::ModuleName& moduleName);
    std::vector<Reference> findAllReferences(const Luau::TypeId ty, std::optional<Luau::Name> property = std::nullopt);
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

    bool updateSourceMap();

    bool isNullWorkspace() const
    {
        return name == "$NULL_WORKSPACE";
    };
};
