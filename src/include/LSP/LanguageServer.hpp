#include <optional>
#include <queue>
#include <condition_variable>

#include "LSP/JsonRpc.hpp"
#include "nlohmann/json.hpp"

#include "Protocol/Structures.hpp"
#include "Protocol/LanguageFeatures.hpp"

#include "LSP/Client.hpp"
#include "LSP/Workspace.hpp"

#include "Thread.hpp"

using json = nlohmann::json;
using namespace json_rpc;
using WorkspaceFolderPtr = std::shared_ptr<WorkspaceFolder>;

#define JSON_REQUIRED_PARAMS(params, method) \
    (!(params) ? throw json_rpc::JsonRpcException(lsp::ErrorCode::InvalidParams, "params not provided for " method) : (params).value())

inline lsp::PositionEncodingKind& positionEncoding()
{
    static lsp::PositionEncodingKind encoding = lsp::PositionEncodingKind::UTF16;
    return encoding;
}

struct InitializationOptions
{
    std::unordered_map<std::string, std::string> fflags{};
};
NLOHMANN_DEFINE_OPTIONAL(InitializationOptions, fflags)

class LanguageServer
{
private:
    // Client is guaranteed to live for the duration of the whole program
    Client* client;
    std::optional<Luau::Config> defaultConfig;
    // A "in memory" workspace folder which doesn't actually have a root.
    // Any files which aren't part of a workspace but are opened will be handled here.
    // This is common if the client has not yet opened a folder
    WorkspaceFolderPtr nullWorkspace;
    std::vector<WorkspaceFolderPtr> workspaceFolders;

    std::vector<json_rpc::JsonRpcMessage> configPostponedMessages;

public:
    explicit LanguageServer(Client* aClient, std::optional<Luau::Config> aDefaultConfig);

    lsp::ServerCapabilities getServerCapabilities();

    /// Finds the workspace which the file belongs to.
    /// If no workspace is found, the file is attached to the null workspace
    WorkspaceFolderPtr findWorkspace(const lsp::DocumentUri& file, bool shouldInitialize = true);

    void onRequest(const id_type& id, const std::string& method, std::optional<json> params);
    void onNotification(const std::string& method, std::optional<json> params);
    void processInputLoop();
    bool requestedShutdown();

    // Visibile for testing only
    void shutdown();

    // Dispatch handlers
private:
    bool allWorkspacesReceivedConfiguration() const;
    void clearCancellationToken(const json_rpc::JsonRpcMessage& msg);
    void handleMessage(const json_rpc::JsonRpcMessage& msg);
    std::optional<json_rpc::JsonRpcMessage> popMessage();

    lsp::InitializeResult onInitialize(const lsp::InitializeParams& params);
    void onInitialized([[maybe_unused]] const lsp::InitializedParams& params);

    void onDidOpenTextDocument(const lsp::DidOpenTextDocumentParams& params);
    void onDidChangeTextDocument(const lsp::DidChangeTextDocumentParams& params);
    void onDidSaveTextDocument(const lsp::DidSaveTextDocumentParams& params);
    void onDidCloseTextDocument(const lsp::DidCloseTextDocumentParams& params);
    void onDidChangeConfiguration(const lsp::DidChangeConfigurationParams& params);
    void onDidChangeWorkspaceFolders(const lsp::DidChangeWorkspaceFoldersParams& params);
    void onDidChangeWatchedFiles(const lsp::DidChangeWatchedFilesParams& params);

    std::vector<lsp::DocumentLink> documentLink(const lsp::DocumentLinkParams& params);
    lsp::DocumentColorResult documentColor(const lsp::DocumentColorParams& params);
    lsp::ColorPresentationResult colorPresentation(const lsp::ColorPresentationParams& params);
    lsp::CodeActionResult codeAction(const lsp::CodeActionParams& params);

    std::optional<std::vector<lsp::DocumentSymbol>> documentSymbol(const lsp::DocumentSymbolParams& params);
    lsp::PartialResponse<lsp::WorkspaceDiagnosticReport> workspaceDiagnostic(const lsp::WorkspaceDiagnosticParams& params);

private:
    bool isInitialized = false;
    bool shutdownRequested = false;

    std::mutex messagesMutex;
    std::condition_variable messagesCv;
    std::queue<json_rpc::JsonRpcMessage> messages;
    Thread messageProcessorThread;
    std::unordered_map<id_type, LSPCancellationToken> cancellationTokens;
};
