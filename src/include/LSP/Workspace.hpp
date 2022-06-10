#pragma once
#include <iostream>
#include <limits.h>
#include "Luau/Frontend.h"
#include "Luau/Autocomplete.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/ToString.h"
#include "Luau/AstQuery.h"
#include "Luau/TypeInfer.h"
#include "Luau/Transpiler.h"
#include "glob/glob.hpp"
#include "LSP/Client.hpp"
#include "LSP/Protocol.hpp"
#include "LSP/Sourcemap.hpp"
#include "LSP/TextDocument.hpp"
#include "LSP/DocumentationParser.hpp"
#include "LSP/WorkspaceFileResolver.hpp"
#include "LSP/LuauExt.hpp"
#include "LSP/Utils.hpp"

class WorkspaceFolder
{
public:
    std::shared_ptr<Client> client;
    std::string name;
    lsp::DocumentUri rootUri;
    WorkspaceFileResolver fileResolver;
    Luau::Frontend frontend;

public:
    WorkspaceFolder(std::shared_ptr<Client> client, const std::string& name, const lsp::DocumentUri& uri)
        : client(client)
        , name(name)
        , rootUri(uri)
        , fileResolver(WorkspaceFileResolver())
        , frontend(Luau::Frontend(&fileResolver, &fileResolver, {true}))
    {
        fileResolver.rootUri = uri;
        setup();
    }

    /// Checks whether a provided file is part of the workspace
    bool isInWorkspace(const lsp::DocumentUri& file);

    void openTextDocument(const lsp::DocumentUri& uri, const lsp::DidOpenTextDocumentParams& params);
    void updateTextDocument(const lsp::DocumentUri& uri, const lsp::DidChangeTextDocumentParams& params);
    void closeTextDocument(const lsp::DocumentUri& uri);

    /// Whether the file has been marked as ignored by any of the ignored lists in the configuration
    bool isIgnoredFile(const std::filesystem::path& path, const std::optional<ClientConfiguration>& givenConfig = std::nullopt);

    lsp::DocumentDiagnosticReport documentDiagnostics(const lsp::DocumentDiagnosticParams& params);

private:
    void endAutocompletion(const lsp::CompletionParams& params);

public:
    std::vector<lsp::CompletionItem> completion(const lsp::CompletionParams& params);

    std::vector<lsp::DocumentLink> documentLink(const lsp::DocumentLinkParams& params);

    std::optional<lsp::Hover> hover(const lsp::HoverParams& params);

    std::optional<lsp::SignatureHelp> signatureHelp(const lsp::SignatureHelpParams& params);

    std::optional<lsp::Location> gotoDefinition(const lsp::DefinitionParams& params);

    std::optional<lsp::Location> gotoTypeDefinition(const lsp::TypeDefinitionParams& params);

    lsp::ReferenceResult references(const lsp::ReferenceParams& params);
    lsp::RenameResult rename(const lsp::RenameParams& params);

    std::optional<std::vector<lsp::DocumentSymbol>> documentSymbol(const lsp::DocumentSymbolParams& params);

    bool updateSourceMap();

private:
    void registerInstanceTypes(Luau::TypeChecker& typeChecker);
    void registerDefinitions(Luau::TypeChecker& typeChecker, const std::filesystem::path& definitionsFile);

    bool isNullWorkspace() const
    {
        return name == "$NULL_WORKSPACE";
    }

    void setup();
};