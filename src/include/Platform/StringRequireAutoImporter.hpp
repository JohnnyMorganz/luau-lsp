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
};

using AliasMap = DenseHashMap<std::string, Luau::Config::AliasInfo>;

std::string requireNameFromModuleName(const Luau::ModuleName& name);
std::optional<std::string> computeBestAliasedPath(const Uri& to, const AliasMap& availableAliases);
std::pair<std::string, SortText::SortTextT> computeRequirePath(const Uri& from, const Uri& to, const AliasMap& availableAliases, ImportRequireStyle importRequireStyle);
void suggestStringRequires(const StringRequireAutoImporterContext& ctx, std::vector<lsp::CompletionItem>& items);
}
