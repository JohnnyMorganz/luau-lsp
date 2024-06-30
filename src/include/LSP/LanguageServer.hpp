#include <optional>
#include <filesystem>

#include "LSP/JsonRpc.hpp"
#include "nlohmann/json.hpp"

#include "Protocol/Structures.hpp"
#include "Protocol/LanguageFeatures.hpp"

#include "LSP/Client.hpp"
#include "LSP/Workspace.hpp"

using json = nlohmann::json;
using namespace json_rpc;
using WorkspaceFolderPtr = std::shared_ptr<WorkspaceFolder>;
using ClientPtr = std::shared_ptr<Client>;

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
    std::unordered_map</* DocumentUri */ std::string, ClientConfiguration> config{};
};
NLOHMANN_DEFINE_OPTIONAL(InitializationOptions, fflags, config)

class LanguageServer
{
private:
    // A "in memory" workspace folder which doesn't actually have a root.
    // Any files which aren't part of a workspace but are opened will be handled here.
    // This is common if the client has not yet opened a folder
    ClientPtr client;
    std::optional<Luau::Config> defaultConfig;
    WorkspaceFolderPtr nullWorkspace;
    std::vector<WorkspaceFolderPtr> workspaceFolders;

    std::vector<json_rpc::JsonRpcMessage> configPostponedMessages;

public:
    explicit LanguageServer(ClientPtr aClient, std::optional<Luau::Config> aDefaultConfig)
        : client(std::move(aClient))
        , defaultConfig(std::move(aDefaultConfig))
        , nullWorkspace(std::make_shared<WorkspaceFolder>(client, "$NULL_WORKSPACE", Uri(), defaultConfig))
    {
    }

    lsp::ServerCapabilities getServerCapabilities();

    /// Finds the workspace which the file belongs to.
    /// If no workspace is found, the file is attached to the null workspace
    WorkspaceFolderPtr findWorkspace(const lsp::DocumentUri& file);

    void onRequest(const id_type& id, const std::string& method, std::optional<json> params);
    void onNotification(const std::string& method, std::optional<json> params);
    void processInputLoop();
    bool requestedShutdown();

    // Dispatch handlers
private:
    lsp::InitializeResult onInitialize(const lsp::InitializeParams& params);
    void onInitialized([[maybe_unused]] const lsp::InitializedParams& params);

    void onDidOpenTextDocument(const lsp::DidOpenTextDocumentParams& params);
    void onDidChangeTextDocument(const lsp::DidChangeTextDocumentParams& params);
    void onDidCloseTextDocument(const lsp::DidCloseTextDocumentParams& params);
    void onDidChangeConfiguration(const lsp::DidChangeConfigurationParams& params);
    void onDidChangeWorkspaceFolders(const lsp::DidChangeWorkspaceFoldersParams& params);
    void onDidChangeWatchedFiles(const lsp::DidChangeWatchedFilesParams& params);

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
    std::optional<std::vector<lsp::DocumentSymbol>> documentSymbol(const lsp::DocumentSymbolParams& params);
    lsp::RenameResult rename(const lsp::RenameParams& params);
    lsp::InlayHintResult inlayHint(const lsp::InlayHintParams& params);
    std::optional<lsp::SemanticTokens> semanticTokens(const lsp::SemanticTokensParams& params);
    lsp::DocumentDiagnosticReport documentDiagnostic(const lsp::DocumentDiagnosticParams& params);
    lsp::PartialResponse<lsp::WorkspaceDiagnosticReport> workspaceDiagnostic(const lsp::WorkspaceDiagnosticParams& params);
    Response onShutdown([[maybe_unused]] const id_type& id);

private:
    bool isInitialized = false;
    bool shutdownRequested = false;
};
