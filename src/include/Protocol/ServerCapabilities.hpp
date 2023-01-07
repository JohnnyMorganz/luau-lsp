#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Protocol/SemanticTokens.hpp"

namespace lsp
{

struct DocumentLinkOptions
{
    bool resolveProvider = false;
};
NLOHMANN_DEFINE_OPTIONAL(DocumentLinkOptions, resolveProvider);

struct SignatureHelpOptions
{
    std::optional<std::vector<std::string>> triggerCharacters;
    std::optional<std::vector<std::string>> retriggerCharacters;
};
NLOHMANN_DEFINE_OPTIONAL(SignatureHelpOptions, triggerCharacters, retriggerCharacters);

enum struct TextDocumentSyncKind
{
    None = 0,
    Full = 1,
    Incremental = 2,
};

struct DiagnosticOptions
{
    std::optional<std::string> identifier;
    bool interFileDependencies = false;
    bool workspaceDiagnostics = false;
};
NLOHMANN_DEFINE_OPTIONAL(DiagnosticOptions, identifier, interFileDependencies, workspaceDiagnostics);

struct WorkspaceFoldersServerCapabilities
{
    bool supported = false;
    bool changeNotifications = false;
};
NLOHMANN_DEFINE_OPTIONAL(WorkspaceFoldersServerCapabilities, supported, changeNotifications);

struct WorkspaceCapabilities
{
    std::optional<WorkspaceFoldersServerCapabilities> workspaceFolders;
    // fileOperations
};
NLOHMANN_DEFINE_OPTIONAL(WorkspaceCapabilities, workspaceFolders);

struct CompletionOptions
{
    std::optional<std::vector<std::string>> triggerCharacters;
    std::optional<std::vector<std::string>> allCommitCharacters;
    bool resolveProvider = false;

    struct CompletionItem
    {
        bool labelDetailsSupport = false;
    };
    std::optional<CompletionItem> completionItem;
};
NLOHMANN_DEFINE_OPTIONAL(CompletionOptions::CompletionItem, labelDetailsSupport);
NLOHMANN_DEFINE_OPTIONAL(CompletionOptions, triggerCharacters, allCommitCharacters, resolveProvider, completionItem);

struct SemanticTokensOptions
{
    SemanticTokensLegend legend;
    bool range = false;
    bool full = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SemanticTokensOptions, legend, range, full);

struct ServerCapabilities
{
    PositionEncodingKind positionEncoding = PositionEncodingKind::UTF16;
    std::optional<TextDocumentSyncKind> textDocumentSync;
    std::optional<CompletionOptions> completionProvider;
    bool hoverProvider = false;
    std::optional<SignatureHelpOptions> signatureHelpProvider;
    bool declarationProvider = false;
    bool definitionProvider = false;
    bool typeDefinitionProvider = false;
    bool implementationProvider = false;
    bool referencesProvider = false;
    bool documentSymbolProvider = false;
    std::optional<DocumentLinkOptions> documentLinkProvider;
    bool colorProvider = false;
    bool renameProvider = false;
    bool inlayHintProvider = false;
    std::optional<DiagnosticOptions> diagnosticProvider;
    std::optional<SemanticTokensOptions> semanticTokensProvider;
    std::optional<WorkspaceCapabilities> workspace;
};
NLOHMANN_DEFINE_OPTIONAL(ServerCapabilities, positionEncoding, textDocumentSync, completionProvider, hoverProvider, signatureHelpProvider,
    declarationProvider, definitionProvider, typeDefinitionProvider, implementationProvider, referencesProvider, documentSymbolProvider,
    documentLinkProvider, colorProvider, renameProvider, inlayHintProvider, diagnosticProvider, semanticTokensProvider, workspace);
} // namespace lsp