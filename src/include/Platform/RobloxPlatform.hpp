#pragma once

#include "LSP/LuauExt.hpp"
#include "Platform/LSPPlatform.hpp"
#include "Platform/AutoImports.hpp"

using json = nlohmann::json;

const std::string kSourcemapGeneratedTag = "@sourcemap-generated";

struct RobloxDefinitionsFileMetadata
{
    std::vector<std::string> CREATABLE_INSTANCES{};
    std::vector<std::string> SERVICES{};
};
NLOHMANN_DEFINE_OPTIONAL(RobloxDefinitionsFileMetadata, CREATABLE_INSTANCES, SERVICES)

struct RobloxFindImportsVisitor : public Luau::LanguageServer::AutoImports::FindImportsVisitor
{
public:
    std::optional<size_t> firstServiceDefinitionLine = std::nullopt;
    std::optional<size_t> lastServiceDefinitionLine = std::nullopt;
    std::map<std::string, Luau::AstStatLocal*> serviceLineMap{};

    size_t findBestLineForService(const std::string& serviceName, size_t minimumLineNumber)
    {
        if (firstServiceDefinitionLine)
            minimumLineNumber = *firstServiceDefinitionLine > minimumLineNumber ? *firstServiceDefinitionLine : minimumLineNumber;

        size_t lineNumber = minimumLineNumber;
        for (auto& [definedService, stat] : serviceLineMap)
        {
            auto location = stat->location.end.line;
            if (definedService < serviceName && location >= lineNumber)
                lineNumber = location + 1;
        }
        return lineNumber;
    }

    bool handleLocal(Luau::AstStatLocal* local, Luau::AstLocal* localName, Luau::AstExpr* expr, unsigned int startLine, unsigned int endLine) override
    {
        if (!isGetService(expr))
            return false;

        firstServiceDefinitionLine = !firstServiceDefinitionLine.has_value() || firstServiceDefinitionLine.value() >= startLine
                                         ? startLine
                                         : firstServiceDefinitionLine.value();
        lastServiceDefinitionLine =
            !lastServiceDefinitionLine.has_value() || lastServiceDefinitionLine.value() <= endLine ? endLine : lastServiceDefinitionLine.value();
        serviceLineMap.emplace(std::string(localName->name.value), local);

        return true;
    }

    [[nodiscard]] size_t getMinimumRequireLine() const override
    {
        if (lastServiceDefinitionLine)
            return *lastServiceDefinitionLine + 1;

        return 0;
    }

    [[nodiscard]] bool shouldPrependNewline(size_t lineNumber) const override
    {
        return lastServiceDefinitionLine && lineNumber - *lastServiceDefinitionLine == 1;
    }
};

struct SourceNode
{
    const SourceNode* parent = nullptr; // Can be null! NOT POPULATED BY SOURCEMAP, must be written to manually
    std::string name;
    std::string className;
    std::vector<std::string> filePaths{};
    std::vector<SourceNode*> children{};
    std::string virtualPath; // NB: NOT POPULATED BY SOURCEMAP, must be written to manually

    // The corresponding TypeId for this sourcemap node
    // A different TypeId is created for each type checker (frontend.typeChecker and frontend.typeCheckerForAutocomplete)
    mutable std::unordered_map<Luau::GlobalTypes const*, Luau::TypeId> tys{}; // NB: NOT POPULATED BY SOURCEMAP, created manually. Can be null!

    SourceNode(std::string name, std::string className, std::vector<std::string> filePaths, std::vector<SourceNode*> children);

    bool isScript() const;
    std::optional<std::string> getScriptFilePath() const;
    Luau::SourceCode::Type sourceCodeType() const;
    std::optional<SourceNode*> findChild(const std::string& name) const;
    std::optional<const SourceNode*> findDescendant(const std::string& name) const;
    // O(n) search for ancestor of name
    std::optional<const SourceNode*> findAncestor(const std::string& name) const;

    static SourceNode* fromJson(const json& j, Luau::TypedAllocator<SourceNode>& allocator);
};

struct PluginNode
{
    std::string name = "";
    std::string className = "";
    std::vector<PluginNode*> children{};

    static PluginNode* fromJson(const json& j, Luau::TypedAllocator<PluginNode>& allocator);
};

class RobloxPlatform : public LSPPlatform
{
private:
    // Plugin-provided DataModel information
    PluginNode* pluginInfo = nullptr;
    Luau::TypedAllocator<PluginNode> pluginNodeAllocator;

    mutable std::unordered_map<Uri, const SourceNode*, UriHash> realPathsToSourceNodes{};
    mutable std::unordered_map<Luau::ModuleName, const SourceNode*> virtualPathsToSourceNodes{};

    std::optional<const SourceNode*> getSourceNodeFromVirtualPath(const Luau::ModuleName& name) const;
    std::optional<const SourceNode*> getSourceNodeFromRealPath(const Uri& name) const;

    static Luau::ModuleName getVirtualPathFromSourceNode(const SourceNode* sourceNode);

    void clearSourcemapTypes();
    bool updateSourceMap();

public:
    // The root source node from a parsed Rojo source map
    SourceNode* rootSourceNode = nullptr;
    Luau::TypedAllocator<SourceNode> sourceNodeAllocator;

    Luau::TypeArena instanceTypes;

    /// For testing only
    bool updateSourceMapFromContents(const std::string& sourceMapContents);
    /// For testing only
    void writePathsToMap(SourceNode* node, const std::string& base);
    std::optional<Uri> getRealPathFromSourceNode(const SourceNode* sourceNode) const;

    void mutateRegisteredDefinitions(Luau::GlobalTypes& globals, std::optional<nlohmann::json> metadata) override;

    void onDidChangeWatchedFiles(const lsp::FileEvent& change) override;

    void setupWithConfiguration(const ClientConfiguration& config) override;

    bool isVirtualPath(const Luau::ModuleName& name) const override
    {
        return name == "game" || name == "ProjectRoot" || Luau::startsWith(name, "game/") || Luau::startsWith(name, "ProjectRoot/");
    }

    std::optional<Luau::ModuleName> resolveToVirtualPath(const Uri& name) const override;

    std::optional<Uri> resolveToRealPath(const Luau::ModuleName& name) const override;

    Luau::SourceCode::Type sourceCodeTypeFromPath(const Uri& path) const override;

    std::optional<std::string> readSourceCode(const Luau::ModuleName& name, const Uri& path) const override;

    std::optional<Luau::ModuleInfo> resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* node) override;

    void updateSourceNodeMap(const std::string& sourceMapContents);

    void handleSourcemapUpdate(Luau::Frontend& frontend, const Luau::GlobalTypes& globals, bool expressiveTypes);

    std::optional<Luau::AutocompleteEntryMap> completionCallback(const std::string& tag, std::optional<const Luau::ExternType*> ctx,
        std::optional<std::string> contents, const Luau::ModuleName& moduleName) override;

    const char* handleSortText(const Luau::Frontend& frontend, const std::string& name, const Luau::AutocompleteEntry& entry,
        const std::unordered_set<std::string>& tags) override;

    std::optional<lsp::CompletionItemKind> handleEntryKind(const Luau::AutocompleteEntry& entry) override;

    void handleSuggestImports(const TextDocument& textDocument, const Luau::SourceModule& module, const ClientConfiguration& config,
        size_t hotCommentsLineNumber, bool completingTypeReferencePrefix, std::vector<lsp::CompletionItem>& items) override;

    lsp::WorkspaceEdit computeOrganiseServicesEdit(const lsp::DocumentUri& uri);
    void handleCodeAction(const lsp::CodeActionParams& params, std::vector<lsp::CodeAction>& items) override;

    lsp::DocumentColorResult documentColor(const TextDocument& textDocument, const Luau::SourceModule& module) override;

    lsp::ColorPresentationResult colorPresentation(const lsp::ColorPresentationParams& params) override;

    void onStudioPluginFullChange(const json& dataModel);
    void onStudioPluginClear();
    bool handleNotification(const std::string& method, std::optional<json> params) override;


    using LSPPlatform::LSPPlatform;
};
