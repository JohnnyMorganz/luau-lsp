#pragma once
#include <iostream>
#include <limits.h>
// #include "glob/glob.hpp"
#include "Luau/Frontend.h"
// #include "Luau/ToString.h"
// #include "Luau/AstQuery.h"
// #include "Luau/TypeInfer.h"
// #include "Luau/Transpiler.h"
#include "Protocol/Structures.hpp"
#include "Protocol/LanguageFeatures.hpp"
#include "Protocol/SignatureHelp.hpp"
#include "Protocol/SemanticTokens.hpp"
#include "LSP/Client.hpp"
// #include "LSP/Sourcemap.hpp"
// #include "LSP/TextDocument.hpp"
// #include "LSP/DocumentationParser.hpp"
#include "LSP/WorkspaceFileResolver.hpp"
// #include "LSP/LuauExt.hpp"
// #include "LSP/Utils.hpp"

class WorkspaceFolder
{
public:
    std::shared_ptr<Client> client;
    std::string name;
    lsp::DocumentUri rootUri;
    WorkspaceFileResolver fileResolver;
    Luau::Frontend frontend;
    bool isConfigured = false;
    Luau::TypeArena instanceTypes;

public:
    WorkspaceFolder(std::shared_ptr<Client> client, const std::string& name, const lsp::DocumentUri& uri)
        : client(client)
        , name(name)
        , rootUri(uri)
        , fileResolver(WorkspaceFileResolver())
        , frontend(Luau::Frontend(&fileResolver, &fileResolver, {true}))
    {
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

private:
    void endAutocompletion(const lsp::CompletionParams& params);
    void suggestImports(const Luau::ModuleName& moduleName, const Luau::Position& position, const ClientConfiguration& config,
        std::vector<lsp::CompletionItem>& result);

public:
    std::vector<std::string> getComments(const Luau::ModuleName& moduleName, const Luau::Location& node);

    std::vector<lsp::CompletionItem> completion(const lsp::CompletionParams& params);

    std::vector<lsp::DocumentLink> documentLink(const lsp::DocumentLinkParams& params);
    lsp::DocumentColorResult documentColor(const lsp::DocumentColorParams& params);
    lsp::ColorPresentationResult colorPresentation(const lsp::ColorPresentationParams& params);

    std::optional<lsp::Hover> hover(const lsp::HoverParams& params);

    std::optional<lsp::SignatureHelp> signatureHelp(const lsp::SignatureHelpParams& params);

    lsp::DefinitionResult gotoDefinition(const lsp::DefinitionParams& params);

    std::optional<lsp::Location> gotoTypeDefinition(const lsp::TypeDefinitionParams& params);

    lsp::ReferenceResult references(const lsp::ReferenceParams& params);
    lsp::RenameResult rename(const lsp::RenameParams& params);
    lsp::InlayHintResult inlayHint(const lsp::InlayHintParams& params);

    std::optional<std::vector<lsp::DocumentSymbol>> documentSymbol(const lsp::DocumentSymbolParams& params);
    std::optional<lsp::SemanticTokens> semanticTokens(const lsp::SemanticTokensParams& params);

    bool updateSourceMap();

    bool isNullWorkspace() const
    {
        return name == "$NULL_WORKSPACE";
    };
};