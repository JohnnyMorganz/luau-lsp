#pragma once

#include "LSP/ClientConfiguration.hpp"
#include "LSP/LuauExt.hpp"
#include "LSP/TextDocument.hpp"
#include "LSP/WorkspaceFileResolver.hpp"
#include "Luau/Ast.h"
#include "Luau/Autocomplete.h"
#include "Luau/Frontend.h"
#include "Luau/GlobalTypes.h"
#include "Luau/Module.h"
#include "Luau/TypeFwd.h"
#include "Protocol/Completion.hpp"
#include "Protocol/LanguageFeatures.hpp"
#include "Protocol/SignatureHelp.hpp"
#include "nlohmann/json.hpp"

#include <memory>
#include <unordered_set>

class WorkspaceFolder;

class LSPPlatform
{
protected:
    WorkspaceFolder* workspaceFolder;

public:
    virtual std::unique_ptr<FindImportsVisitor> getImportVisitor()
    {
        return std::make_unique<FindImportsVisitor>();
    }

    virtual void mutateRegisteredDefinitions(Luau::GlobalTypes& globals, std::optional<nlohmann::json> metadata) {}

    virtual void handleSourcemapUpdate(
        Luau::Frontend& frontend, const Luau::GlobalTypes& globals, const WorkspaceFileResolver& fileResolver, bool expressiveTypes)
    {
    }

    virtual void handleCompletion(
        const TextDocument& textDocument, const Luau::SourceModule& module, Luau::Position position, std::vector<lsp::CompletionItem>& items)
    {
    }

    virtual std::optional<Luau::AutocompleteEntryMap> completionCallback(
        const std::string& tag, std::optional<const Luau::ClassType*> ctx, std::optional<std::string> contents, const Luau::ModuleName& moduleName)
    {
        return std::nullopt;
    }

    virtual const char* handleSortText(
        const Luau::Frontend& frontend, const std::string& name, const Luau::AutocompleteEntry& entry, const std::unordered_set<std::string>& tags)
    {
        return nullptr;
    }

    virtual std::optional<lsp::CompletionItemKind> handleEntryKind(const Luau::AutocompleteEntry& entry)
    {
        return std::nullopt;
    }

    virtual void handleSuggestImports(const ClientConfiguration& config, FindImportsVisitor* importsVisitor, size_t hotCommentsLineNumber,
        bool completingTypeReferencePrefix, std::vector<lsp::CompletionItem>& items)
    {
    }

    virtual void handleRequireAutoImport(const std::string& requirePath, size_t lineNumber, bool isRelative, const ClientConfiguration& config,
        FindImportsVisitor* importsVisitor, size_t hotCommentsLineNumber, std::vector<lsp::TextEdit>& textEdits)
    {
    }

    virtual void handleSignatureHelp(
        const TextDocument& textDocument, const Luau::SourceModule& module, Luau::Position position, lsp::SignatureHelp& signatureHelp)
    {
    }

    virtual void handleCodeAction(const lsp::CodeActionParams& params, std::vector<lsp::CodeAction>& items) {}

    virtual lsp::DocumentColorResult documentColor(const TextDocument& textDocument, const Luau::SourceModule& module)
    {
        return {};
    }

    virtual lsp::ColorPresentationResult colorPresentation(const lsp::ColorPresentationParams& params)
    {
        return {};
    }

    virtual std::optional<lsp::Hover> handleHover(const TextDocument& textDocument, const Luau::SourceModule& module, Luau::Position position)
    {
        return std::nullopt;
    }

    virtual bool handleNotification(const std::string& method, std::optional<json> params)
    {
        return false;
    }

    static std::unique_ptr<LSPPlatform> getPlatform(const ClientConfiguration& config, WorkspaceFolder* workspaceFolder = nullptr);

    LSPPlatform(const LSPPlatform& copyFrom) = delete;
    LSPPlatform(LSPPlatform&&) = delete;
    LSPPlatform& operator=(const LSPPlatform& copyFrom) = delete;
    LSPPlatform& operator=(LSPPlatform&&) = delete;

    LSPPlatform(WorkspaceFolder* workspaceFolder = nullptr);
    virtual ~LSPPlatform() = default;
};
