#include <string>
#include <optional>
#include <variant>

#include "nlohmann/json.hpp"

using json = nlohmann::json;
using URI = std::string;         // TODO: URI
using DocumentUri = std::string; // TODO: URI

// #define OPTIONAL_GET(j, prop, name, type) \
//     if (j.contains(name)) \
//         prop = j.at(name).get<type>(prop);

// #define OPTIONAL_SET(j, prop, name) \
//     if (prop) \
//         j[name] = prop.value();

namespace lsp
{
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

struct ClientCapabilities
{
    // TODO
};
void to_json(json& j, const ClientCapabilities& p)
{
    j;
    p;
};
void from_json(const json& j, ClientCapabilities& p)
{
    j;
    p;
};

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
    // rootPath
    // rootUri
    std::optional<json> initializationOptions;
    ClientCapabilities capabilities;
    // traceValue
    // workspaceFolders
};

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
void to_json(json& j, const CompletionOptions& p)
{
    j = json{{"resolveProvider", p.resolveProvider}};
    if (p.triggerCharacters)
        j["triggerCharacters"] = p.triggerCharacters.value();
    if (p.allCommitCharacters)
        j["allCommitCharacters"] = p.allCommitCharacters.value();
    if (p.completionItem)
        j["completionItem"] = {{"labelDetailsSupport", p.completionItem->labelDetailsSupport}};
}

enum struct TextDocumentSyncKind
{
    None = 0,
    Full = 1,
    Incremental = 2,
};

struct ServerCapabilities
{
    std::optional<TextDocumentSyncKind> textDocumentSync;
    std::optional<CompletionOptions> completionProvider;
};

void to_json(json& j, const ServerCapabilities& p)
{
    j = json{};
    if (p.textDocumentSync)
        j["textDocumentSync"] = p.textDocumentSync.value();
    if (p.completionProvider)
        j["completionProvider"] = p.completionProvider.value();
}

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

void to_json(json& j, const InitializeResult& p)
{
    j = json{
        {"capabilities", p.capabilities},
    };
    if (p.serverInfo)
    {
        j["serverInfo"] = {{"name", p.serverInfo->name}};
        if (p.serverInfo->version)
            j["serverInfo"] = {{"name", p.serverInfo->name}, {"version", p.serverInfo->version.value()}};
    }
}

struct InitializedParams
{
};
void to_json(json& j, const InitializedParams& p)
{
    j;
    p;
};
void from_json(const json& j, InitializedParams& p)
{
    j;
    p;
};

struct Position
{
    unsigned int line;
    unsigned int character;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Position, line, character);

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
    int version;
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
    int version;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(VersionedTextDocumentIdentifier, uri, version);

struct Location
{
    DocumentUri uri;
    Range range;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Location, uri, range);

struct DidOpenTextDocumentParams
{
    TextDocumentItem textDocument;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DidOpenTextDocumentParams, textDocument);

struct TextDocumentContentChangeEvent
{
    // If only text is provided, then its considered to be the whole document
    std::optional<Range> range;
    std::string text;
};
void from_json(const json& j, TextDocumentContentChangeEvent& p)
{
    j.at("text").get_to(p.text);
    if (j.contains("range"))
        p.range = j.at("range").get<Range>();
}
void to_json(json& j, const TextDocumentContentChangeEvent& p)
{
    j = json{
        {"text", p.text},
    };
    if (p.range)
        j["range"] = p.range.value();
}

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
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DidCloseTextDocumentParams, textDocument);

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
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DiagnosticRelatedInformation, location, message);

struct CodeDescription
{
    URI href;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CodeDescription, href);

struct Diagnostic
{
    Range range;
    std::optional<DiagnosticSeverity> severity;
    std::optional<std::variant<std::string, int>> code;
    std::optional<CodeDescription> codeDescription;
    std::optional<std::string> source;
    std::string message;
    std::optional<std::vector<DiagnosticTag>> tags;
    std::optional<std::vector<DiagnosticRelatedInformation>> relatedInformation;
    // data?
};
void to_json(json& j, const Diagnostic& p)
{
    j = json{{"range", p.range}, {"message", p.message}};
    if (p.severity)
        j["severity"] = p.severity.value();
    if (p.code)
    {
        if (std::holds_alternative<int>(p.code.value()))
        {
            j["code"] = std::get<int>(p.code.value());
        }
        else
        {
            j["code"] = std::get<std::string>(p.code.value());
        }
    }
    if (p.codeDescription)
        j["codeDescription"] = p.codeDescription.value();
    if (p.source)
        j["source"] = p.source.value();
    if (p.tags)
        j["tags"] = p.tags.value();
    if (p.relatedInformation)
        j["relatedInformation"] = p.relatedInformation.value();
}

struct PublishDiagnosticsParams
{
    DocumentUri uri;
    std::optional<int> version;
    std::vector<Diagnostic> diagnostics;
};
void to_json(json& j, const PublishDiagnosticsParams& p)
{
    j = json{{"uri", p.uri}, {"diagnostics", p.diagnostics}};
    if (p.version)
        j["version"] = p.version.value();
}

enum struct TraceValue
{
    Off,
    Messages,
    Verbose,
};
NLOHMANN_JSON_SERIALIZE_ENUM(TraceValue, {
                                             {TraceValue::Off, "off"},
                                             {TraceValue::Messages, "messages"},
                                             {TraceValue::Verbose, "verbose"},
                                         });

struct SetTraceParams
{
    TraceValue value;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetTraceParams, value);

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

} // namespace lsp