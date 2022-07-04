#pragma once
#include <string>
#include <optional>
#include <variant>

#include "nlohmann/json.hpp"
#include "LSP/Uri.hpp"

using json = nlohmann::json;

// We create our own macro with special to_json support for std::optional / nullptr
// Note: instead of converting nullptr to a JSON null, this macro omits the field completely (similar to undefined)
// WARNING: explicit nulls will be lost! If nulls are necessary (and no undefineds), then use the standard macro
#define NLOHMANN_JSON_TO_OPTIONAL(v1) \
    { \
        json val = nlohmann_json_t.v1; \
        if (val != nullptr) \
            nlohmann_json_j[#v1] = val; \
    };
#define NLOHMANN_DEFINE_OPTIONAL(Type, ...) \
    inline void to_json(nlohmann::json& nlohmann_json_j, const Type& nlohmann_json_t) \
    { \
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO_OPTIONAL, __VA_ARGS__)) \
    } \
    inline void from_json(const nlohmann::json& nlohmann_json_j, Type& nlohmann_json_t) \
    { \
        Type nlohmann_json_default_obj; \
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM_WITH_DEFAULT, __VA_ARGS__)) \
    }

// Define serializer/deserializer for std::optional and std::variant
namespace nlohmann
{
template<typename T>
struct adl_serializer<std::optional<T>>
{
    static void to_json(json& j, const std::optional<T>& opt)
    {
        if (opt == std::nullopt)
            j = nullptr;
        else
            j = *opt;
    }

    static void from_json(const json& j, std::optional<T>& opt)
    {
        if (j.is_null())
            opt = std::nullopt;
        else
            opt = j.get<T>();
    }
};

// string | int is a common variant, so we will just special case it
template<>
struct adl_serializer<std::variant<std::string, int>>
{
    static void to_json(json& j, const std::variant<std::string, int>& data)
    {
        if (auto str = std::get_if<std::string>(&data))
        {
            j = *str;
        }
        else if (auto num = std::get_if<int>(&data))
        {
            j = *num;
        }
    }

    static void from_json(const json& j, std::variant<std::string, int>& data)
    {
        // TODO: handle nicely?
        assert(j.is_string() || j.is_number());

        if (j.is_string())
            data = j.get<std::string>();
        else if (j.is_number())
            data = j.get<int>();
    }
};

// Same for int | bool
template<>
struct adl_serializer<std::variant<int, bool>>
{
    static void to_json(json& j, const std::variant<int, bool>& data)
    {
        if (auto str = std::get_if<bool>(&data))
        {
            j = *str;
        }
        else if (auto num = std::get_if<int>(&data))
        {
            j = *num;
        }
    }

    static void from_json(const json& j, std::variant<int, bool>& data)
    {
        // TODO: handle nicely?
        assert(j.is_boolean() || j.is_number());

        if (j.is_boolean())
            data = j.get<bool>();
        else if (j.is_number())
            data = j.get<int>();
    }
};
} // namespace nlohmann


namespace lsp
{
using URI = Uri;
using DocumentUri = Uri;

// Alias a std::optional to PartialResponse
// If it contains std::nullopt, we shouldn't send a result.
template<typename T>
using PartialResponse = std::optional<T>;

enum struct ErrorCode
{
    // JSON RPC errors
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,

    // LSP Errors
    ServerNotInitialized = -32002,
    UnknownErrorCode = -32001,
    RequestFailed = -32803,
    ServerCancelled = -32802,
    ContentModified = -32801,
    RequestCancelled = -32800,
};

enum struct PositionEncodingKind
{
    UTF8,
    UTF16,
    UTF32
};
NLOHMANN_JSON_SERIALIZE_ENUM(
    PositionEncodingKind, {{PositionEncodingKind::UTF8, "utf-8"}, {PositionEncodingKind::UTF16, "utf-16"}, {PositionEncodingKind::UTF32, "utf-32"}});


using ProgressToken = std::variant<std::string, int>;

struct PartialResultParams
{
    std::optional<ProgressToken> partialResultToken;
};

// TODO: make this generic over value
struct ProgressParams
{
    ProgressToken token;
    json value;
};
NLOHMANN_DEFINE_OPTIONAL(ProgressParams, token, value);

struct DiagnosticClientCapabilities
{
    bool dynamicRegistration = false;
    bool relatedDocumentSupport = false;
};
NLOHMANN_DEFINE_OPTIONAL(DiagnosticClientCapabilities, dynamicRegistration, relatedDocumentSupport);

struct DiagnosticWorkspaceClientCapabilities
{
    /**
     * Whether the client implementation supports a refresh request sent from
     * the server to the client.
     *
     * Note that this event is global and will force the client to refresh all
     * pulled diagnostics currently shown. It should be used with absolute care
     * and is useful for situation where a server for example detects a project
     * wide change that requires such a calculation.
     */
    bool refreshSupport = false;
};
NLOHMANN_DEFINE_OPTIONAL(DiagnosticWorkspaceClientCapabilities, refreshSupport);

struct TextDocumentClientCapabilities
{
    std::optional<DiagnosticClientCapabilities> diagnostic;
};
NLOHMANN_DEFINE_OPTIONAL(TextDocumentClientCapabilities, diagnostic);

struct DidChangeConfigurationClientCapabilities
{
    /**
     * Did change configuration notification supports dynamic registration.
     */
    bool dynamicRegistration = false;
};
NLOHMANN_DEFINE_OPTIONAL(DidChangeConfigurationClientCapabilities, dynamicRegistration);

struct DidChangeWatchedFilesClientCapabilities
{
    /**
     * Did change watched files notification supports dynamic registration.
     * Please note that the current protocol doesn't support static
     * configuration for file changes from the server side.
     */
    bool dynamicRegistration = false;

    /**
     * Whether the client has support for relative patterns
     * or not.
     *
     * @since 3.17.0
     */
    bool relativePatternSupport = false;
};
NLOHMANN_DEFINE_OPTIONAL(DidChangeWatchedFilesClientCapabilities, dynamicRegistration, relativePatternSupport);

struct ClientWorkspaceCapabilities
{
    /**
     * Capabilities specific to the `workspace/didChangeConfiguration`
     * notification.
     */
    std::optional<DidChangeConfigurationClientCapabilities> didChangeConfiguration;

    /**
     * Capabilities specific to the `workspace/didChangeWatchedFiles`
     * notification.
     */
    std::optional<DidChangeWatchedFilesClientCapabilities> didChangeWatchedFiles;

    /**
     * The client supports `workspace/configuration` requests.
     *
     * @since 3.6.0
     */
    bool configuration = false;

    /**
     * Client workspace capabilities specific to diagnostics.
     *
     * @since 3.17.0.
     */
    std::optional<DiagnosticWorkspaceClientCapabilities> diagnostics;
};
NLOHMANN_DEFINE_OPTIONAL(ClientWorkspaceCapabilities, didChangeConfiguration, didChangeWatchedFiles, configuration, diagnostics);

struct ClientGeneralCapabilities
{
    /**
     * The position encodings supported by the client. Client and server
     * have to agree on the same position encoding to ensure that offsets
     * (e.g. character position in a line) are interpreted the same on both
     * side.
     *
     * To keep the protocol backwards compatible the following applies: if
     * the value 'utf-16' is missing from the array of position encodings
     * servers can assume that the client supports UTF-16. UTF-16 is
     * therefore a mandatory encoding.
     *
     * If omitted it defaults to ['utf-16'].
     *
     * Implementation considerations: since the conversion from one encoding
     * into another requires the content of the file / line the conversion
     * is best done where the file is read which is usually on the server
     * side.
     *
     * @since 3.17.0
     */
    std::optional<std::vector<PositionEncodingKind>> positionEncodings;
};
NLOHMANN_DEFINE_OPTIONAL(ClientGeneralCapabilities, positionEncodings);

struct ClientCapabilities
{
    /**
     * Text document specific client capabilities.
     */
    std::optional<TextDocumentClientCapabilities> textDocument;

    /**
     * Workspace specific client capabilities.
     */
    std::optional<ClientWorkspaceCapabilities> workspace;

    /**
     * General client capabilities.
     *
     * @since 3.16.0
     */
    std::optional<ClientGeneralCapabilities> general;

    // TODO
    // notebook
    // window
};
NLOHMANN_DEFINE_OPTIONAL(ClientCapabilities, textDocument, workspace, general);

struct WorkspaceFolder
{
    DocumentUri uri;
    std::string name;
};
NLOHMANN_DEFINE_OPTIONAL(WorkspaceFolder, uri, name);

enum struct TraceValue
{
    Off,
    Messages,
    Verbose,
};
NLOHMANN_JSON_SERIALIZE_ENUM(TraceValue, {{TraceValue::Off, "off"}, {TraceValue::Messages, "messages"}, {TraceValue::Verbose, "verbose"}});

struct InitializeParams
{
    struct ClientInfo
    {
        std::string name;
        std::optional<std::string> version;
    };

    std::optional<int> processId;
    std::optional<ClientInfo> clientInfo;
    std::optional<std::string> locale;
    // TODO: rootPath (deprecated?)
    std::optional<DocumentUri> rootUri;
    std::optional<json> initializationOptions;
    ClientCapabilities capabilities;
    TraceValue trace = TraceValue::Off;
    std::optional<std::vector<WorkspaceFolder>> workspaceFolders;
};
NLOHMANN_DEFINE_OPTIONAL(InitializeParams::ClientInfo, name, version);
NLOHMANN_DEFINE_OPTIONAL(InitializeParams, processId, clientInfo, locale, rootUri, initializationOptions, capabilities, trace, workspaceFolders);

struct Registration
{
    std::string id;
    std::string method;
    json registerOptions = nullptr;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Registration, id, method, registerOptions);

struct RegistrationParams
{
    std::vector<Registration> registrations;
};
NLOHMANN_DEFINE_OPTIONAL(RegistrationParams, registrations);

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
    bool renameProvider = false;
    std::optional<DiagnosticOptions> diagnosticProvider;
    std::optional<WorkspaceCapabilities> workspace;
};
NLOHMANN_DEFINE_OPTIONAL(ServerCapabilities, positionEncoding, textDocumentSync, completionProvider, hoverProvider, signatureHelpProvider,
    declarationProvider, definitionProvider, typeDefinitionProvider, implementationProvider, referencesProvider, documentSymbolProvider,
    documentLinkProvider, renameProvider, diagnosticProvider, workspace);

struct InitializeResult
{
    struct ServerInfo
    {
        std::string name;
        std::optional<std::string> version;
    };

    ServerCapabilities capabilities;
    std::optional<ServerInfo> serverInfo;
};
NLOHMANN_DEFINE_OPTIONAL(InitializeResult::ServerInfo, name, version);
NLOHMANN_DEFINE_OPTIONAL(InitializeResult, capabilities, serverInfo);

struct InitializedParams
{
};
inline void from_json(const json&, InitializedParams&){};

struct Position
{
    size_t line;
    size_t character;
    friend bool operator==(const Position& lhs, const Position& rhs);
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Position, line, character);
inline bool operator==(const Position& lhs, const Position& rhs)
{
    return lhs.line == rhs.line && lhs.character == rhs.character;
}

struct Range
{
    Position start;
    Position end;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Range, start, end);

struct TextDocumentItem
{
    DocumentUri uri;
    std::string languageId;
    size_t version;
    std::string text;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TextDocumentItem, uri, languageId, version, text);

struct TextDocumentIdentifier
{
    DocumentUri uri;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TextDocumentIdentifier, uri);

struct VersionedTextDocumentIdentifier : TextDocumentIdentifier
{
    size_t version;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(VersionedTextDocumentIdentifier, uri, version);

struct TextDocumentPositionParams
{
    TextDocumentIdentifier textDocument;
    Position position;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TextDocumentPositionParams, textDocument, position);

struct TextEdit
{
    Range range;
    std::string newText;
};
NLOHMANN_DEFINE_OPTIONAL(TextEdit, range, newText);

struct Location
{
    DocumentUri uri;
    Range range;
};
NLOHMANN_DEFINE_OPTIONAL(Location, uri, range);

struct Command
{
    std::string title;
    std::string command;
    std::vector<json> arguments;
};
NLOHMANN_DEFINE_OPTIONAL(Command, title, command, arguments);

struct DidOpenTextDocumentParams
{
    TextDocumentItem textDocument;
};
NLOHMANN_DEFINE_OPTIONAL(DidOpenTextDocumentParams, textDocument);

struct TextDocumentContentChangeEvent
{
    // If only text is provided, then its considered to be the whole document
    std::optional<Range> range = std::nullopt;
    std::string text;
};
NLOHMANN_DEFINE_OPTIONAL(TextDocumentContentChangeEvent, range, text);

struct DidChangeTextDocumentParams
{
    VersionedTextDocumentIdentifier textDocument;
    std::vector<TextDocumentContentChangeEvent> contentChanges;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DidChangeTextDocumentParams, textDocument, contentChanges);

struct DidCloseTextDocumentParams
{
    TextDocumentIdentifier textDocument;
};
NLOHMANN_DEFINE_OPTIONAL(DidCloseTextDocumentParams, textDocument);

struct DidChangeConfigurationParams
{
    json settings;
};
NLOHMANN_DEFINE_OPTIONAL(DidChangeConfigurationParams, settings);

struct ConfigurationItem
{
    std::optional<DocumentUri> scopeUri;
    std::optional<std::string> section;
};
NLOHMANN_DEFINE_OPTIONAL(ConfigurationItem, scopeUri, section);

struct ConfigurationParams
{
    std::vector<ConfigurationItem> items;
};
NLOHMANN_DEFINE_OPTIONAL(ConfigurationParams, items);

using GetConfigurationResponse = std::vector<json>;

using Pattern = std::string;
using GlobPattern = Pattern; // | RelativePattern

enum WatchKind
{
    Create = 1,
    Change = 2,
    Delete = 4,
};

struct FileSystemWatcher
{
    GlobPattern globPattern;
    int kind = WatchKind::Create | WatchKind::Change | WatchKind::Delete;
};
NLOHMANN_DEFINE_OPTIONAL(FileSystemWatcher, globPattern, kind);

struct DidChangeWatchedFilesRegistrationOptions
{
    std::vector<FileSystemWatcher> watchers;
};
NLOHMANN_DEFINE_OPTIONAL(DidChangeWatchedFilesRegistrationOptions, watchers);

enum struct FileChangeType
{
    Created = 1,
    Changed = 2,
    Deleted = 3,
};

struct FileEvent
{
    DocumentUri uri;
    FileChangeType type;
};
NLOHMANN_DEFINE_OPTIONAL(FileEvent, uri, type);

struct DidChangeWatchedFilesParams
{
    std::vector<FileEvent> changes;
};
NLOHMANN_DEFINE_OPTIONAL(DidChangeWatchedFilesParams, changes);

enum struct DiagnosticSeverity
{
    Error = 1,
    Warning = 2,
    Information = 3,
    Hint = 4,
};

enum struct DiagnosticTag
{
    Unnecessary = 1,
    Deprecated = 2,
};

struct DiagnosticRelatedInformation
{
    Location location;
    std::string message;
};
NLOHMANN_DEFINE_OPTIONAL(DiagnosticRelatedInformation, location, message);

struct CodeDescription
{
    URI href;
};
NLOHMANN_DEFINE_OPTIONAL(CodeDescription, href);

struct Diagnostic
{
    Range range;
    std::optional<DiagnosticSeverity> severity;
    std::optional<std::variant<std::string, int>> code;
    std::optional<CodeDescription> codeDescription;
    std::optional<std::string> source;
    std::string message;
    std::vector<DiagnosticTag> tags;
    std::vector<DiagnosticRelatedInformation> relatedInformation;
    // data?
};
NLOHMANN_DEFINE_OPTIONAL(Diagnostic, range, severity, code, codeDescription, source, message, tags, relatedInformation);

struct PublishDiagnosticsParams
{
    DocumentUri uri;
    std::optional<size_t> version;
    std::vector<Diagnostic> diagnostics;
};
NLOHMANN_DEFINE_OPTIONAL(PublishDiagnosticsParams, uri, version, diagnostics);

struct DocumentDiagnosticParams
{
    TextDocumentIdentifier textDocument;
    std::optional<std::string> identifier;
    std::optional<std::string> previousResultId;
};
NLOHMANN_DEFINE_OPTIONAL(DocumentDiagnosticParams, textDocument, identifier, previousResultId);

enum struct DocumentDiagnosticReportKind
{
    Full,
    Unchanged,
};
NLOHMANN_JSON_SERIALIZE_ENUM(
    DocumentDiagnosticReportKind, {{DocumentDiagnosticReportKind::Full, "full"}, {DocumentDiagnosticReportKind::Unchanged, "unchanged"}});

// TODO: we slightly stray away from the specification here for simplicity
// The specification defines separated types FullDocumentDiagnosticReport and UnchangedDocumentDiagnosticReport, depending on the kind
struct SingleDocumentDiagnosticReport
{
    DocumentDiagnosticReportKind kind = DocumentDiagnosticReportKind::Full;
    std::optional<std::string> resultId; // NB: this MUST be present if kind == Unchanged
    std::vector<Diagnostic> items;       // NB: this MUST NOT be present if kind == Unchanged
};
NLOHMANN_DEFINE_OPTIONAL(SingleDocumentDiagnosticReport, kind, resultId, items);

struct RelatedDocumentDiagnosticReport : SingleDocumentDiagnosticReport
{
    std::unordered_map<std::string /* DocumentUri */, SingleDocumentDiagnosticReport> relatedDocuments;
};
NLOHMANN_DEFINE_OPTIONAL(RelatedDocumentDiagnosticReport, kind, resultId, items, relatedDocuments);

using DocumentDiagnosticReport = RelatedDocumentDiagnosticReport;

// struct FullDocumentDiagnosticReport
// {
//     DocumentDiagnosticReportKind kind = DocumentDiagnosticReportKind::Full;
//     std::optional<std::string> resultId;
//     std::vector<Diagnostic> items;
// };
// struct UnchangedDocumentDiagnosticReport
// {
//     DocumentDiagnosticReportKind kind = DocumentDiagnosticReportKind::Unchanged;
//     std::string resultId;
// };
// using SingleDocumentDiagnosticReport = std::variant<FullDocumentDiagnosticReport, UnchangedDocumentDiagnosticReport>;
// struct RelatedFullDocumentDiagnosticReport : FullDocumentDiagnosticReport
// {
//     std::map<std::string /* DocumentUri */, SingleDocumentDiagnosticReport> relatedDocuments;
// };
// struct RelatedUnchangedDocumentDiagnosticReport : UnchangedDocumentDiagnosticReport
// {
//     std::map<std::string /* DocumentUri */, SingleDocumentDiagnosticReport> relatedDocuments;
// };
// using DocumentDiagnosticReport = std::variant<RelatedFullDocumentDiagnosticReport, RelatedUnchangedDocumentDiagnosticReport>;

struct PreviousResultId
{
    DocumentUri uri;
    std::optional<std::string> value;
};
NLOHMANN_DEFINE_OPTIONAL(PreviousResultId, uri, value);

struct WorkspaceDiagnosticParams : PartialResultParams
{
    std::optional<std::string> identifier;
    std::vector<PreviousResultId> previousResultIds;
};
NLOHMANN_DEFINE_OPTIONAL(WorkspaceDiagnosticParams, partialResultToken, identifier, previousResultIds);

struct WorkspaceDocumentDiagnosticReport : SingleDocumentDiagnosticReport
{
    DocumentUri uri;
    std::optional<size_t> version;
};
NLOHMANN_DEFINE_OPTIONAL(WorkspaceDocumentDiagnosticReport, kind, resultId, items, uri, version);

struct WorkspaceDiagnosticReport
{
    std::vector<WorkspaceDocumentDiagnosticReport> items;
};
NLOHMANN_DEFINE_OPTIONAL(WorkspaceDiagnosticReport, items);

struct WorkspaceDiagnosticReportPartialResult
{
    std::vector<WorkspaceDocumentDiagnosticReport> items;
};
NLOHMANN_DEFINE_OPTIONAL(WorkspaceDiagnosticReportPartialResult, items);

struct DiagnosticServerCancellationData
{
    bool retriggerRequest = true;
};
NLOHMANN_DEFINE_OPTIONAL(DiagnosticServerCancellationData, retriggerRequest);

enum struct CompletionTriggerKind
{
    Invoked = 1,
    TriggerCharacter = 2,
    TriggerForIncompleteCompletions = 3
};

struct CompletionContext
{
    CompletionTriggerKind triggerKind;
    std::optional<std::string> triggerCharacter;
};
NLOHMANN_DEFINE_OPTIONAL(CompletionContext, triggerKind, triggerCharacter);

struct CompletionParams : TextDocumentPositionParams
{
    std::optional<CompletionContext> context;
};
NLOHMANN_DEFINE_OPTIONAL(CompletionParams, textDocument, position, context);

enum struct InsertTextFormat
{
    PlainText = 1,
    Snippet = 2,
};

enum struct CompletionItemTag
{
    Deprecated = 1,
};

enum struct InsertTextMode
{
    AsIs = 1,
    AdjustIndentation = 2,
};

struct CompletionItemLabelDetails
{
    std::optional<std::string> detail;
    std::optional<std::string> description;
};
NLOHMANN_DEFINE_OPTIONAL(CompletionItemLabelDetails, detail, description);

enum struct CompletionItemKind
{
    Text = 1,
    Method = 2,
    Function = 3,
    Constructor = 4,
    Field = 5,
    Variable = 6,
    Class = 7,
    Interface = 8,
    Module = 9,
    Property = 10,
    Unit = 11,
    Value = 12,
    Enum = 13,
    Keyword = 14,
    Snippet = 15,
    Color = 16,
    File = 17,
    Reference = 18,
    Folder = 19,
    EnumMember = 20,
    Constant = 21,
    Struct = 22,
    Event = 23,
    Operator = 24,
    TypeParameter = 25,
};

enum struct MarkupKind
{
    PlainText,
    Markdown,
};
NLOHMANN_JSON_SERIALIZE_ENUM(MarkupKind, {{MarkupKind::PlainText, "plaintext"}, {MarkupKind::Markdown, "markdown"}});

struct MarkupContent
{
    MarkupKind kind;
    std::string value;
};
NLOHMANN_DEFINE_OPTIONAL(MarkupContent, kind, value);

struct CompletionItem
{
    std::string label;
    std::optional<CompletionItemLabelDetails> labelDetails;
    std::optional<CompletionItemKind> kind;
    std::optional<std::vector<CompletionItemTag>> tags;
    std::optional<std::string> detail;
    std::optional<MarkupContent> documentation;
    bool deprecated = false;
    bool preselect = false;
    std::optional<std::string> sortText;
    std::optional<std::string> filterText;
    std::optional<std::string> insertText;
    InsertTextFormat insertTextFormat = InsertTextFormat::PlainText;
    std::optional<InsertTextMode> insertTextMode;
    std::optional<TextEdit> textEdit;
    std::optional<std::string> textEditString;
    std::vector<TextEdit> additionalTextEdits;
    std::optional<std::vector<std::string>> commitCharacters;
    std::optional<Command> command;
    // TODO: data?
};
NLOHMANN_DEFINE_OPTIONAL(CompletionItem, label, labelDetails, kind, tags, detail, documentation, deprecated, preselect, sortText, filterText,
    insertText, insertTextFormat, insertTextMode, textEdit, textEditString, additionalTextEdits, commitCharacters, command);

struct DocumentLinkParams
{
    TextDocumentIdentifier textDocument;
};
NLOHMANN_DEFINE_OPTIONAL(DocumentLinkParams, textDocument);

struct DocumentLink
{
    Range range;
    DocumentUri target; // TODO: potentially optional if we resolve later
    std::optional<std::string> tooltip;
    // std::optional<json> data; // for resolver
};
NLOHMANN_DEFINE_OPTIONAL(DocumentLink, range, target, tooltip);

struct HoverParams : TextDocumentPositionParams
{
};

struct Hover
{
    MarkupContent contents;
    std::optional<Range> range;
};
NLOHMANN_DEFINE_OPTIONAL(Hover, contents, range);

struct ParameterInformation
{
    std::string label;
    std::optional<MarkupContent> documentation;
};
NLOHMANN_DEFINE_OPTIONAL(ParameterInformation, label, documentation);

struct SignatureInformation
{
    std::string label;
    std::optional<MarkupContent> documentation;
    std::optional<std::vector<ParameterInformation>> parameters;
    std::optional<size_t> activeParameter;
};
NLOHMANN_DEFINE_OPTIONAL(SignatureInformation, label, documentation, parameters, activeParameter);

struct SignatureHelp
{
    std::vector<SignatureInformation> signatures;
    size_t activeSignature = 0;
    size_t activeParameter = 0;
};
NLOHMANN_DEFINE_OPTIONAL(SignatureHelp, signatures, activeSignature, activeParameter);

enum struct SignatureHelpTriggerKind
{
    Invoked = 1,
    TriggerCharacter = 2,
    ContentChange = 3,
};

struct SignatureHelpContext
{
    SignatureHelpTriggerKind triggerKind;
    std::optional<std::string> triggerCharacter;
    bool isRetrigger;
    std::optional<SignatureHelp> activeSignatureHelp;
};
NLOHMANN_DEFINE_OPTIONAL(SignatureHelpContext, triggerKind, triggerCharacter, isRetrigger, activeSignatureHelp);

struct SignatureHelpParams : TextDocumentPositionParams
{
    std::optional<SignatureHelpContext> context;
};

struct DefinitionParams : TextDocumentPositionParams
{
};
struct TypeDefinitionParams : TextDocumentPositionParams
{
};

struct ReferenceContext
{
    bool includeDeclaration;
};
NLOHMANN_DEFINE_OPTIONAL(ReferenceContext, includeDeclaration);

struct ReferenceParams : TextDocumentPositionParams
{
    ReferenceContext context;
};
NLOHMANN_DEFINE_OPTIONAL(ReferenceParams, textDocument, position, context);

using ReferenceResult = std::optional<std::vector<Location>>;

struct DocumentSymbolParams
{
    TextDocumentIdentifier textDocument;
};
NLOHMANN_DEFINE_OPTIONAL(DocumentSymbolParams, textDocument);

enum struct SymbolKind
{
    File = 1,
    Module = 2,
    Namespace = 3,
    Package = 4,
    Class = 5,
    Method = 6,
    Property = 7,
    Field = 8,
    Constructor = 9,
    Enum = 10,
    Interface = 11,
    Function = 12,
    Variable = 13,
    Constant = 14,
    String = 15,
    Number = 16,
    Boolean = 17,
    Array = 18,
    Object = 19,
    Key = 20,
    Null = 21,
    EnumMember = 22,
    Struct = 23,
    Event = 24,
    Operator = 25,
    TypeParameter = 26,
};

enum struct SymbolTag
{
    Deprecated = 1,
};

struct DocumentSymbol
{
    std::string name;
    std::optional<std::string> detail;
    SymbolKind kind;
    std::vector<SymbolTag> tags;
    bool deprecated = false;
    Range range;
    Range selectionRange;
    std::vector<DocumentSymbol> children;
};
NLOHMANN_DEFINE_OPTIONAL(DocumentSymbol, name, detail, kind, tags, deprecated, range, selectionRange, children);

struct WorkspaceEdit
{
    // TODO: this is optional and there are other options provided
    std::unordered_map<std::string /* DocumentUri */, std::vector<TextEdit>> changes;
};
NLOHMANN_DEFINE_OPTIONAL(WorkspaceEdit, changes);

struct RenameParams : TextDocumentPositionParams
{
    std::string newName;
};
NLOHMANN_DEFINE_OPTIONAL(RenameParams, textDocument, position, newName);

using RenameResult = std::optional<WorkspaceEdit>;

struct WorkspaceFoldersChangeEvent
{
    std::vector<WorkspaceFolder> added;
    std::vector<WorkspaceFolder> removed;
};
NLOHMANN_DEFINE_OPTIONAL(WorkspaceFoldersChangeEvent, added, removed);

struct DidChangeWorkspaceFoldersParams
{
    WorkspaceFoldersChangeEvent event;
};
NLOHMANN_DEFINE_OPTIONAL(DidChangeWorkspaceFoldersParams, event);

struct ApplyWorkspaceEditParams
{
    std::optional<std::string> label;
    WorkspaceEdit edit;
};
NLOHMANN_DEFINE_OPTIONAL(ApplyWorkspaceEditParams, label, edit);

struct ApplyWorkspaceEditResult
{
    bool applied;
    std::optional<std::string> failureReason;
    std::optional<size_t> failedChange;
};
NLOHMANN_DEFINE_OPTIONAL(ApplyWorkspaceEditResult, applied, failureReason, failedChange);

struct SetTraceParams
{
    TraceValue value;
};
NLOHMANN_DEFINE_OPTIONAL(SetTraceParams, value);

enum struct MessageType
{
    Error = 1,
    Warning = 2,
    Info = 3,
    Log = 4,
};

struct LogMessageParams
{
    MessageType type;
    std::string message;
};
NLOHMANN_DEFINE_OPTIONAL(LogMessageParams, type, message);

struct ShowMessageParams
{
    MessageType type;
    std::string message;
};
NLOHMANN_DEFINE_OPTIONAL(ShowMessageParams, type, message);

} // namespace lsp