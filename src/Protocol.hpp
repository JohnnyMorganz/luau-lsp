#include <string>
#include <optional>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace lsp
{
enum ErrorCode
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

// struct Position
// {
//     unsigned int line;
//     unsigned int character;
// };

// struct Range
// {
//     Position start;
//     Position end;
// };

struct ClientCapabilities
{
    // TODO
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


struct ServerCapabilities
{
    std::optional<CompletionOptions> completionProvider;
};

void to_json(json& j, const ServerCapabilities& p)
{
    j = json{};
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

enum MessageType
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