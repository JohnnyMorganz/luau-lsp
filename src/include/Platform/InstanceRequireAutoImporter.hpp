#pragma once

#include "Luau/Frontend.h"
#include "Platform/AutoImports.hpp"
#include "LSP/Workspace.hpp"

#include <string>

class RobloxPlatform;

namespace Luau::LanguageServer::AutoImports
{

struct RobloxFindImportsVisitor : FindImportsVisitor
{
    std::optional<size_t> firstServiceDefinitionLine = std::nullopt;
    std::optional<size_t> lastServiceDefinitionLine = std::nullopt;
    std::map<std::string, Luau::AstStatLocal*> serviceLineMap{};

    size_t findBestLineForService(const std::string& serviceName, size_t minimumLineNumber) const
    {
        if (firstServiceDefinitionLine)
            minimumLineNumber = *firstServiceDefinitionLine > minimumLineNumber ? *firstServiceDefinitionLine : minimumLineNumber;

        size_t lineNumber = minimumLineNumber;
        for (const auto& [definedService, stat] : serviceLineMap)
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


struct InstanceRequireAutoImporterContext
{
    Luau::ModuleName from;
    Luau::NotNull<const TextDocument> textDocument;

    Luau::NotNull<const Luau::Frontend> frontend;
    Luau::NotNull<const WorkspaceFolder> workspaceFolder;
    Luau::NotNull<const ClientCompletionImportsConfiguration> config;

    size_t hotCommentsLineNumber = 0;
    Luau::NotNull<const RobloxFindImportsVisitor> importsVisitor;

    Luau::NotNull<const RobloxPlatform> platform;

    std::optional<std::function<bool(const std::string&)>> moduleFilter;
};

/// Result of computing a string require import
struct InstanceRequireResult
{
    std::string variableName; // e.g., "MyModule"
    Luau::ModuleName moduleName;
    std::string requirePath;                                          // e.g., "@shared/MyModule" or "./MyModule"
    std::optional<std::pair<std::string, lsp::TextEdit>> serviceEdit; // TextEdit to include a service, if necessary
    lsp::TextEdit edit;                                               // The actual text edit
    const char* sortText;                                             // For completion sorting
};

/// Create a text edit for inserting a Roblox service import (e.g., `local Players = game:GetService("Players")`)
lsp::TextEdit createServiceTextEdit(const std::string& name, size_t lineNumber, bool appendNewline = false);
/// Optimise an absolute require path by removing the "game/" prefix (e.g., "game/ReplicatedStorage/Foo" -> "ReplicatedStorage/Foo")
std::string optimiseAbsoluteRequire(const std::string& path);

std::vector<InstanceRequireResult> computeAllInstanceRequires(const InstanceRequireAutoImporterContext& ctx);
void suggestInstanceRequires(const InstanceRequireAutoImporterContext& ctx, std::vector<lsp::CompletionItem>& items);
} // namespace Luau::LanguageServer::AutoImports
