#pragma once

#include "Luau/Frontend.h"
#include "Platform/AutoImports.hpp"
#include "LSP/Completion.hpp"
#include "LSP/Workspace.hpp"

namespace Luau::LanguageServer::AutoImports
{
struct StringRequireAutoImporterContext
{
    Luau::ModuleName from;
    Luau::NotNull<const TextDocument> textDocument;

    Luau::NotNull<const Luau::Frontend> frontend;
    Luau::NotNull<const WorkspaceFolder> workspaceFolder;
    Luau::NotNull<const ClientCompletionImportsConfiguration> config;

    size_t hotCommentsLineNumber = 0;
    Luau::NotNull<const FindImportsVisitor> importsVisitor;

    std::optional<std::function<bool(const Luau::ModuleName&, const std::string&)>> moduleFilter;
};

/// Result of computing a string require import
struct StringRequireResult
{
    std::string variableName; // e.g., "MyModule"
    Luau::ModuleName moduleName;
    std::string requirePath; // e.g., "@shared/MyModule" or "./MyModule"
    lsp::TextEdit edit;      // The actual text edit
    const char* sortText;    // For completion sorting
};

using AliasMap = DenseHashMap<std::string, Luau::Config::AliasInfo>;

std::string requireNameFromModuleName(const Luau::ModuleName& name);
std::optional<std::string> computeBestAliasedPath(const Uri& to, const AliasMap& availableAliases);
std::pair<std::string, SortText::SortTextT> computeRequirePath(
    const Uri& from, Uri to, const AliasMap& availableAliases, ImportRequireStyle importRequireStyle);
std::vector<StringRequireResult> computeAllStringRequires(const StringRequireAutoImporterContext& ctx);
void suggestStringRequires(const StringRequireAutoImporterContext& ctx, std::vector<lsp::CompletionItem>& items);
} // namespace Luau::LanguageServer::AutoImports
