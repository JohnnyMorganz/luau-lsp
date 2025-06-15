#pragma once
#include <optional>
#include <unordered_map>
#include <utility>
#include "Luau/FileResolver.h"
#include "Luau/StringUtils.h"
#include "Luau/Config.h"
#include "LSP/Client.hpp"
#include "LSP/Uri.hpp"
#include "LSP/TextDocument.hpp"
#include "Platform/LSPPlatform.hpp"


// A wrapper around a text document pointer
// A text document might be temporarily created for the purposes of this function
// in which case it should be deleted once the ptr goes out of scope.
// I don't think we can use a unique_ptr here, because a managed text document is not owned
// NOTE: document may still be nil!
struct TextDocumentPtr
{
private:
    const TextDocument* document = nullptr;
    bool isTemporary = false;

public:
    explicit TextDocumentPtr(const TextDocument* document)
        : document(document)
    {
    }

    explicit TextDocumentPtr(const lsp::DocumentUri& uri, const std::string& languageId, const std::string& content)
        : document(new TextDocument(uri, languageId, 0, content))
        , isTemporary(true)
    {
    }

    explicit operator bool() const
    {
        return document != nullptr;
    }

    const TextDocument* operator*() const
    {
        return document;
    }

    const TextDocument* operator->() const
    {
        return document;
    }

    ~TextDocumentPtr()
    {
        if (isTemporary)
            delete document;
    }

    TextDocumentPtr(const TextDocumentPtr& other) = delete;
    TextDocumentPtr& operator=(const TextDocumentPtr& other) = delete;

    TextDocumentPtr(TextDocumentPtr&& other) noexcept
        : document(std::exchange(other.document, nullptr))
        , isTemporary(std::exchange(other.isTemporary, false))
    {
    }

    TextDocumentPtr& operator=(TextDocumentPtr&& other) noexcept
    {
        std::swap(document, other.document);
        std::swap(isTemporary, other.isTemporary);
        return *this;
    }
};

struct WorkspaceFileResolver
    : Luau::FileResolver
    , Luau::ConfigResolver
{
private:
    mutable std::unordered_map<Uri, Luau::Config, UriHash> configCache{};

public:
    Luau::Config defaultConfig;
    std::shared_ptr<BaseClient> client;
    Uri rootUri;

    LSPPlatform* platform = nullptr;

    // Currently opened files where content is managed by client
    mutable std::unordered_map<Uri, TextDocument, UriHash> managedFiles{};

    WorkspaceFileResolver()
    {
        defaultConfig.mode = Luau::Mode::Nonstrict;
    }

    // Create a WorkspaceFileResolver with a specific default configuration
    explicit WorkspaceFileResolver(Luau::Config defaultConfig)
        : defaultConfig(std::move(defaultConfig)){};

    /// The file is managed by the client, so FS will be out of date
    const TextDocument* getTextDocument(const lsp::DocumentUri& uri) const;
    const TextDocument* getTextDocumentFromModuleName(const Luau::ModuleName& name) const;

    TextDocumentPtr getOrCreateTextDocumentFromModuleName(const Luau::ModuleName& name);

    // Return the corresponding module name from a file Uri
    // We first try and find a virtual file path which matches it, and return that. Otherwise, we use the file system path
    Luau::ModuleName getModuleName(const Uri& name) const;
    Uri getUri(const Luau::ModuleName& moduleName) const;

    std::optional<Luau::SourceCode> readSource(const Luau::ModuleName& name) override;
    std::optional<Luau::ModuleInfo> resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* node) override;
    std::string getHumanReadableModuleName(const Luau::ModuleName& name) const override;
    const Luau::Config& getConfig(const Luau::ModuleName& name) const override;
    void clearConfigCache();

    static std::optional<std::string> parseConfig(const Uri& configPath, const std::string& contents, Luau::Config& result, bool compat = false);

private:
    const Luau::Config& readConfigRec(const Uri& path) const;
};
