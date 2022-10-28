#include <optional>
#include <filesystem>
#include "LSP/Client.hpp"
#include "LSP/Protocol.hpp"
#include "LSP/JsonRpc.hpp"
#include "LSP/Uri.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/DocumentationParser.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;
using namespace json_rpc;
using Response = json;
using WorkspaceFolderPtr = std::shared_ptr<WorkspaceFolder>;
using ClientPtr = std::shared_ptr<Client>;

inline lsp::PositionEncodingKind& positionEncoding()
{
    static lsp::PositionEncodingKind encoding = lsp::PositionEncodingKind::UTF16;
    return encoding;
}

class LanguageServer
{
public:
    // A "in memory" workspace folder which doesn't actually have a root.
    // Any files which aren't part of a workspace but are opened will be handled here.
    // This is common if the client has not yet opened a folder
    WorkspaceFolderPtr nullWorkspace;
    std::vector<WorkspaceFolderPtr> workspaceFolders;
    ClientPtr client;

    LanguageServer(std::vector<std::filesystem::path> definitionsFiles, std::optional<std::filesystem::path> documentationFile)
        : client(std::make_shared<Client>())
    {
        client->definitionsFiles = definitionsFiles;
        client->documentationFile = documentationFile;
        parseDocumentation(documentationFile, client->documentation, client);
        nullWorkspace = std::make_shared<WorkspaceFolder>(client, "$NULL_WORKSPACE", Uri());
    }

    lsp::ServerCapabilities getServerCapabilities();

    /// Finds the workspace which the file belongs to.
    /// If no workspace is found, the file is attached to the null workspace
    WorkspaceFolderPtr findWorkspace(const lsp::DocumentUri file);

    void onRequest(const id_type& id, const std::string& method, std::optional<json> params);
    void onNotification(const std::string& method, std::optional<json> params);
    void processInputLoop();
    bool requestedShutdown();

    // Dispatch handlers
private:
    lsp::InitializeResult onInitialize(const lsp::InitializeParams& params);
    void onInitialized(const lsp::InitializedParams& params);

    void pushDiagnostics(WorkspaceFolderPtr& workspace, const lsp::DocumentUri& uri, const size_t version);
    void recomputeDiagnostics(WorkspaceFolderPtr& workspace, const ClientConfiguration& config);

    void onDidOpenTextDocument(const lsp::DidOpenTextDocumentParams& params);
    void onDidChangeTextDocument(const lsp::DidChangeTextDocumentParams& params);
    void onDidCloseTextDocument(const lsp::DidCloseTextDocumentParams& params);
    void onDidChangeConfiguration(const lsp::DidChangeConfigurationParams& params);
    void onDidChangeWorkspaceFolders(const lsp::DidChangeWorkspaceFoldersParams& params);
    void onDidChangeWatchedFiles(const lsp::DidChangeWatchedFilesParams& params);

    void onStudioPluginFullChange(const PluginNode& dataModel);
    void onStudioPluginClear();

    std::vector<lsp::CompletionItem> completion(const lsp::CompletionParams& params);
    std::vector<lsp::DocumentLink> documentLink(const lsp::DocumentLinkParams& params);

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
    Response onShutdown(const id_type& id);

private:
    bool isInitialized = false;
    bool shutdownRequested = false;
};