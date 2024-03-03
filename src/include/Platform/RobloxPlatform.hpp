#pragma once

#include "LSP/LuauExt.hpp"
#include "Platform/LSPPlatform.hpp"

using json = nlohmann::json;
using SourceNodePtr = std::shared_ptr<struct SourceNode>;
using PluginNodePtr = std::shared_ptr<struct PluginNode>;

struct RobloxDefinitionsFileMetadata
{
    std::vector<std::string> CREATABLE_INSTANCES{};
    std::vector<std::string> SERVICES{};
};
NLOHMANN_DEFINE_OPTIONAL(RobloxDefinitionsFileMetadata, CREATABLE_INSTANCES, SERVICES)

struct RobloxFindImportsVisitor : public FindImportsVisitor
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

    bool handleLocal(Luau::AstStatLocal* local, Luau::AstLocal* localName, Luau::AstExpr* expr, unsigned int line) override
    {
        if (!isGetService(expr))
            return false;

        firstServiceDefinitionLine =
            !firstServiceDefinitionLine.has_value() || firstServiceDefinitionLine.value() >= line ? line : firstServiceDefinitionLine.value();
        lastServiceDefinitionLine =
            !lastServiceDefinitionLine.has_value() || lastServiceDefinitionLine.value() <= line ? line : lastServiceDefinitionLine.value();
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
    std::weak_ptr<struct SourceNode> parent; // Can be null! NOT POPULATED BY SOURCEMAP, must be written to manually
    std::string name;
    std::string className;
    std::vector<std::filesystem::path> filePaths{};
    std::vector<SourceNodePtr> children{};
    std::string virtualPath; // NB: NOT POPULATED BY SOURCEMAP, must be written to manually
    // The corresponding TypeId for this sourcemap node
    // A different TypeId is created for each type checker (frontend.typeChecker and frontend.typeCheckerForAutocomplete)
    std::unordered_map<Luau::GlobalTypes const*, Luau::TypeId> tys{}; // NB: NOT POPULATED BY SOURCEMAP, created manually. Can be null!

    bool isScript();
    std::optional<std::filesystem::path> getScriptFilePath();
    Luau::SourceCode::Type sourceCodeType() const;
    std::optional<SourceNodePtr> findChild(const std::string& name);
    // O(n) search for ancestor of name
    std::optional<SourceNodePtr> findAncestor(const std::string& name);
};

static void from_json(const json& j, SourceNode& p)
{
    j.at("name").get_to(p.name);
    j.at("className").get_to(p.className);

    if (j.contains("filePaths"))
        j.at("filePaths").get_to(p.filePaths);

    if (j.contains("children"))
    {
        for (auto& child : j.at("children"))
        {
            p.children.push_back(std::make_shared<SourceNode>(child.get<SourceNode>()));
        }
    }
}

struct PluginNode
{
    std::string name = "";
    std::string className = "";
    std::vector<PluginNodePtr> children{};
};

static void from_json(const json& j, PluginNode& p)
{
    j.at("Name").get_to(p.name);
    j.at("ClassName").get_to(p.className);

    if (j.contains("Children"))
    {
        for (auto& child : j.at("Children"))
        {
            p.children.push_back(std::make_shared<PluginNode>(child.get<PluginNode>()));
        }
    }
}

class RobloxPlatform : public LSPPlatform
{
private:
    // Plugin-provided DataModel information
    PluginNodePtr pluginInfo;

    // The root source node from a parsed Rojo source map
    SourceNodePtr rootSourceNode;

    mutable std::unordered_map<std::string, SourceNodePtr> realPathsToSourceNodes{};
    mutable std::unordered_map<Luau::ModuleName, SourceNodePtr> virtualPathsToSourceNodes{};

    std::optional<SourceNodePtr> getSourceNodeFromVirtualPath(const Luau::ModuleName& name) const;
    std::optional<SourceNodePtr> getSourceNodeFromRealPath(const std::string& name) const;
    std::optional<std::filesystem::path> getRealPathFromSourceNode(const SourceNodePtr& sourceNode) const;
    static Luau::ModuleName getVirtualPathFromSourceNode(const SourceNodePtr& sourceNode);

    bool updateSourceMap();
    void writePathsToMap(const SourceNodePtr& node, const std::string& base);

public:
    Luau::TypeArena instanceTypes;

    void mutateRegisteredDefinitions(Luau::GlobalTypes& globals, std::optional<nlohmann::json> metadata) override;

    void onDidChangeWatchedFiles(const lsp::FileEvent& change) override;

    void setupWithConfiguration(const ClientConfiguration& config) override;

    bool isVirtualPath(const Luau::ModuleName& name) const override
    {
        return name == "game" || name == "ProjectRoot" || Luau::startsWith(name, "game/") || Luau::startsWith(name, "ProjectRoot/");
    }

    std::optional<Luau::ModuleName> resolveToVirtualPath(const std::string& name) const override;

    std::optional<std::filesystem::path> resolveToRealPath(const Luau::ModuleName& name) const override;

    Luau::SourceCode::Type sourceCodeTypeFromPath(const std::filesystem::path& path) const override;

    std::optional<std::string> readSourceCode(const Luau::ModuleName& name, const std::filesystem::path& path) const override;

    std::optional<Luau::ModuleInfo> resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* node) override;

    void updateSourceNodeMap(const std::string& sourceMapContents);

    void handleSourcemapUpdate(Luau::Frontend& frontend, const Luau::GlobalTypes& globals, bool expressiveTypes);

    std::optional<Luau::AutocompleteEntryMap> completionCallback(const std::string& tag, std::optional<const Luau::ClassType*> ctx,
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

    void onStudioPluginFullChange(const PluginNode& dataModel);
    void onStudioPluginClear();
    bool handleNotification(const std::string& method, std::optional<json> params) override;

    using LSPPlatform::LSPPlatform;
};
