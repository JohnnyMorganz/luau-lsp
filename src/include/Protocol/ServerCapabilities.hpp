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
    /**
     * The server has support for workspace folders
     */
    bool supported = false;

    /**
     * Whether the server wants to receive workspace folder
     * change notifications.
     *
     * If a string is provided, the string is treated as an ID
     * under which the notification is registered on the client
     * side. The ID can be used to unregister for these events
     * using the `client/unregisterCapability` request.
     */
    bool changeNotifications = false;
};
NLOHMANN_DEFINE_OPTIONAL(WorkspaceFoldersServerCapabilities, supported, changeNotifications);

/**
 * A pattern kind describing if a glob pattern matches a file a folder or
 * both.
 *
 * @since 3.16.0
 */
enum struct FileOperationPatternKind
{
    /**
     * The pattern matches a file only.
     */
    File,
    /**
     * The pattern matches a folder only.
     */
    Folder,
};
NLOHMANN_JSON_SERIALIZE_ENUM(FileOperationPatternKind, {{FileOperationPatternKind::File, "file"}, {FileOperationPatternKind::Folder, "folder"}});

struct FileOperationPatternOptions
{
    /**
     * The pattern should be matched ignoring casing.
     */
    bool ignoreCase = false;
};
NLOHMANN_DEFINE_OPTIONAL(FileOperationPatternOptions, ignoreCase);

/**
 * A pattern to describe in which file operation requests or notifications
 * the server is interested in.
 *
 * @since 3.16.0
 */
struct FileOperationPattern
{
    /**
     * The glob pattern to match. Glob patterns can have the following syntax:
     * - `*` to match one or more characters in a path segment
     * - `?` to match on one character in a path segment
     * - `**` to match any number of path segments, including none
     * - `{}` to group sub patterns into an OR expression. (e.g. `**​/*.{ts,js}`
     *   matches all TypeScript and JavaScript files)
     * - `[]` to declare a range of characters to match in a path segment
     *   (e.g., `example.[0-9]` to match on `example.0`, `example.1`, …)
     * - `[!...]` to negate a range of characters to match in a path segment
     *   (e.g., `example.[!0-9]` to match on `example.a`, `example.b`, but
     *   not `example.0`)
     */
    std::string glob;

    /**
     * Whether to match files or folders with this pattern.
     *
     * Matches both if undefined.
     */
    std::optional<FileOperationPatternKind> matches = std::nullopt;

    /**
     * Additional options used during matching.
     */
    std::optional<FileOperationPatternOptions> options = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(FileOperationPattern, glob, matches, options);

/**
 * A filter to describe in which file operation requests or notifications
 * the server is interested in.
 *
 * @since 3.16.0
 */
struct FileOperationFilter
{
    /**
     * A Uri like `file` or `untitled`.
     */
    std::optional<std::string> scheme;
    /**
     * The actual file operation pattern.
     */
    FileOperationPattern pattern;
};
NLOHMANN_DEFINE_OPTIONAL(FileOperationFilter, scheme, pattern);


/**
 * The options to register for file operations.
 *
 * @since 3.16.0
 */
struct FileOperationRegistrationOptions
{
    /**
     * The actual filters.
     */
    std::vector<FileOperationFilter> filters;
};
NLOHMANN_DEFINE_OPTIONAL(FileOperationRegistrationOptions, filters);


struct FileOperations
{
    /**
     * The server is interested in receiving didCreateFiles
     * notifications.
     */
    std::optional<FileOperationRegistrationOptions> didCreate = std::nullopt;

    /**
     * The server is interested in receiving willCreateFiles requests.
     */
    std::optional<FileOperationRegistrationOptions> willCreate = std::nullopt;

    /**
     * The server is interested in receiving didRenameFiles
     * notifications.
     */
    std::optional<FileOperationRegistrationOptions> didRename = std::nullopt;

    /**
     * The server is interested in receiving willRenameFiles requests.
     */
    std::optional<FileOperationRegistrationOptions> willRename = std::nullopt;

    /**
     * The server is interested in receiving didDeleteFiles file
     * notifications.
     */
    std::optional<FileOperationRegistrationOptions> didDelete = std::nullopt;

    /**
     * The server is interested in receiving willDeleteFiles file
     * requests.
     */
    std::optional<FileOperationRegistrationOptions> willDelete = std::nullopt;
};

struct WorkspaceCapabilities
{
    /**
     * The server supports workspace folder.
     *
     * @since 3.6.0
     */
    std::optional<WorkspaceFoldersServerCapabilities> workspaceFolders;
    /**
     * The server is interested in file notifications/requests.
     *
     * @since 3.16.0
     */
    std::optional<FileOperations> fileOperations;
};
NLOHMANN_DEFINE_OPTIONAL(WorkspaceCapabilities, workspaceFolders, fileOperations);

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