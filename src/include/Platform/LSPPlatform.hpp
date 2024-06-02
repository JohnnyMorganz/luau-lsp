#pragma once

#include "LSP/ClientConfiguration.hpp"
#include "LSP/TextDocument.hpp"
#include "Luau/Ast.h"
#include "Luau/Autocomplete.h"
#include "Luau/FileResolver.h"
#include "Luau/Frontend.h"
#include "Luau/GlobalTypes.h"
#include "Luau/Module.h"
#include "Luau/TypeFwd.h"
#include "Protocol/CodeAction.hpp"
#include "Protocol/Completion.hpp"
#include "Protocol/LanguageFeatures.hpp"
#include "Protocol/SignatureHelp.hpp"
#include "Protocol/Workspace.hpp"
#include "nlohmann/json.hpp"

#include <filesystem>
#include <memory>
#include <unordered_set>

class WorkspaceFolder;
struct WorkspaceFileResolver;

class LSPPlatform
{
protected:
    WorkspaceFileResolver* fileResolver;
    WorkspaceFolder* workspaceFolder;

public:
    virtual void mutateRegisteredDefinitions(Luau::GlobalTypes& globals, std::optional<nlohmann::json> metadata) {}

    virtual void onDidChangeWatchedFiles(const lsp::FileEvent& change) {}

    virtual void setupWithConfiguration(const ClientConfiguration& config) {}

    /// The name points to a virtual path (i.e. for Roblox, game/ or ProjectRoot/)
    [[nodiscard]] virtual bool isVirtualPath(const Luau::ModuleName& name) const
    {
        return false;
    }

    [[nodiscard]] virtual std::optional<Luau::ModuleName> resolveToVirtualPath(const std::string& name) const
    {
        return std::nullopt;
    }

    [[nodiscard]] virtual std::optional<std::filesystem::path> resolveToRealPath(const Luau::ModuleName& name) const
    {
        return name;
    }

    [[nodiscard]] virtual Luau::SourceCode::Type sourceCodeTypeFromPath(const std::filesystem::path& path) const
    {
        return Luau::SourceCode::Type::Module;
    }

    [[nodiscard]] virtual std::optional<std::string> readSourceCode(const Luau::ModuleName& name, const std::filesystem::path& path) const;

    std::optional<Luau::ModuleInfo> resolveStringRequire(const Luau::ModuleInfo* context, const std::string& requiredString);
    virtual std::optional<Luau::ModuleInfo> resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* node);

    virtual void handleCompletion(
        const TextDocument& textDocument, const Luau::SourceModule& module, Luau::Position position, std::vector<lsp::CompletionItem>& items)
    {
    }

    virtual std::optional<Luau::AutocompleteEntryMap> completionCallback(
        const std::string& tag, std::optional<const Luau::ClassType*> ctx, std::optional<std::string> contents, const Luau::ModuleName& moduleName);

    virtual const char* handleSortText(
        const Luau::Frontend& frontend, const std::string& name, const Luau::AutocompleteEntry& entry, const std::unordered_set<std::string>& tags)
    {
        return nullptr;
    }

    virtual std::optional<lsp::CompletionItemKind> handleEntryKind(const Luau::AutocompleteEntry& entry)
    {
        return std::nullopt;
    }

    virtual void handleSuggestImports(const TextDocument& textDocument, const Luau::SourceModule& module, const ClientConfiguration& config,
        size_t hotCommentsLineNumber, bool completingTypeReferencePrefix, std::vector<lsp::CompletionItem>& items)
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

    static std::unique_ptr<LSPPlatform> getPlatform(
        const ClientConfiguration& config, WorkspaceFileResolver* fileResolver, WorkspaceFolder* workspaceFolder = nullptr);

    LSPPlatform(const LSPPlatform& copyFrom) = delete;
    LSPPlatform(LSPPlatform&&) = delete;
    LSPPlatform& operator=(const LSPPlatform& copyFrom) = delete;
    LSPPlatform& operator=(LSPPlatform&&) = delete;

    LSPPlatform(WorkspaceFileResolver* fileResolver = nullptr, WorkspaceFolder* workspaceFolder = nullptr);
    virtual ~LSPPlatform() = default;

private:
    [[nodiscard]] std::filesystem::path getRequireBasePath(std::optional<Luau::ModuleName> fileModuleName) const;
};

std::optional<std::filesystem::path> resolveDirectoryAlias(
    const std::filesystem::path& rootPath, const std::unordered_map<std::string, std::string>& directoryAliases, const std::string& str);
