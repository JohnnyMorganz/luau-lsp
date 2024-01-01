#pragma once

#include "LSP/LuauExt.hpp"
#include "Platform/LSPPlatform.hpp"

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

class RobloxPlatform : public LSPPlatform
{
private:
    // Plugin-provided DataModel information
    PluginNodePtr pluginInfo;

public:
    Luau::TypeArena instanceTypes;

    std::unique_ptr<FindImportsVisitor> getImportVisitor() override
    {
        return std::make_unique<RobloxFindImportsVisitor>();
    }

    void mutateRegisteredDefinitions(Luau::GlobalTypes& globals, std::optional<nlohmann::json> metadata) override;

    void handleSourcemapUpdate(
        Luau::Frontend& frontend, const Luau::GlobalTypes& globals, const WorkspaceFileResolver& fileResolver, bool expressiveTypes) override;

    std::optional<Luau::AutocompleteEntryMap> completionCallback(const std::string& tag, std::optional<const Luau::ClassType*> ctx,
        std::optional<std::string> contents, const Luau::ModuleName& moduleName) override;

    const char* handleSortText(const Luau::Frontend& frontend, const std::string& name, const Luau::AutocompleteEntry& entry,
        const std::unordered_set<std::string>& tags) override;

    std::optional<lsp::CompletionItemKind> handleEntryKind(const Luau::AutocompleteEntry& entry) override;

    void handleSuggestImports(const ClientConfiguration& config, FindImportsVisitor* importsVisitor, size_t hotCommentsLineNumber,
        bool completingTypeReferencePrefix, std::vector<lsp::CompletionItem>& items) override;

    void handleRequireAutoImport(const std::string& requirePath, size_t lineNumber, bool isRelative, const ClientConfiguration& config,
        FindImportsVisitor* importsVisitor, size_t hotCommentsLineNumber, std::vector<lsp::TextEdit>& textEdits) override;

    lsp::WorkspaceEdit computeOrganiseServicesEdit(const lsp::DocumentUri& uri);
    void handleCodeAction(const lsp::CodeActionParams& params, std::vector<lsp::CodeAction>& items) override;

    lsp::DocumentColorResult documentColor(const TextDocument& textDocument, const Luau::SourceModule& module) override;

    lsp::ColorPresentationResult colorPresentation(const lsp::ColorPresentationParams& params) override;

    void onStudioPluginFullChange(const PluginNode& dataModel);
    void onStudioPluginClear();
    bool handleNotification(const std::string& method, std::optional<json> params) override;

    using LSPPlatform::LSPPlatform;
};
