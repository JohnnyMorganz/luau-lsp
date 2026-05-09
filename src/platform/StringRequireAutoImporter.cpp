#include "Platform/StringRequireAutoImporter.hpp"

#include "Platform/LSPPlatform.hpp"
#include "Platform/RobloxPlatform.hpp"

namespace Luau::LanguageServer::AutoImports
{

namespace
{
    ScriptContext getScriptContext(const StringRequireAutoImporterContext& ctx, const Luau::ModuleName& name)
    {
        if (ctx.workspaceFolder->platform)
        {
            auto platform = dynamic_cast<RobloxPlatform*>(ctx.workspaceFolder->platform.get());
            if (platform)
            {
                auto it = platform->virtualPathsToSourceNodes.find(name);
                if (it != platform->virtualPathsToSourceNodes.end())
                {
                    return it->second->getScriptContext();
                }
            }
        }
        return ScriptContext::Shared;
    }
}

std::string requireNameFromModuleName(const Luau::ModuleName& name)
{
    auto fileName = name;

    if (isInitLuauFile(Uri::file(name)))
        fileName = getParentPath(name).value_or(name);

#ifdef _WIN32
    if (const auto slashPos = fileName.find_last_of("\\/"); slashPos != std::string::npos)
        fileName = fileName.substr(slashPos + 1);
#else
    if (const auto slashPos = fileName.find_last_of('/'); slashPos != std::string::npos)
        fileName = fileName.substr(slashPos + 1);
#endif
    fileName = removeSuffix(fileName, ".luau");
    fileName = removeSuffix(fileName, ".lua");
    return makeValidVariableName(fileName);
}

// Resolves the best alias path
std::optional<std::string> computeBestAliasedPath(const Uri& to, const AliasMap& availableAliases)
{
    std::optional<std::string> bestAliasedPath = std::nullopt;

    for (const auto& [aliasName, aliasInfo] : availableAliases)
    {
        auto aliasLocation = resolveAliasLocation(aliasInfo);

        if (!aliasLocation.isAncestorOf(to))
            continue;

        auto remainder = to.lexicallyRelative(aliasLocation);
        auto aliasedPath = "@" + aliasInfo.originalCase;
        if (!remainder.empty() && remainder != ".")
            aliasedPath += "/" + remainder;

        if (!bestAliasedPath || aliasedPath.size() < bestAliasedPath->size())
            bestAliasedPath = aliasedPath;
    }

    if (bestAliasedPath)
    {
        bestAliasedPath = removeSuffix(*bestAliasedPath, ".luau");
        bestAliasedPath = removeSuffix(*bestAliasedPath, ".lua");
    }

    return bestAliasedPath;
}

/// Resolves to the most appropriate style of string require based off configuration / heuristics
std::pair<std::string, SortText::SortTextT> computeRequirePath(
    const Uri& from, Uri to, const AliasMap& availableAliases, ImportRequireStyle importRequireStyle)
{
    auto fromParent = from.parent();

    if (isInitLuauFile(to))
        to = to.parent().value_or(to);

    if (importRequireStyle != ImportRequireStyle::AlwaysRelative)
    {
        auto bestAliasPath = computeBestAliasedPath(to, availableAliases);
        if (bestAliasPath)
        {
            if (importRequireStyle == ImportRequireStyle::AlwaysAbsolute)
                return {*bestAliasPath, SortText::AutoImportsAbsolute};
            else
            {
                auto toParent = to.parent();
                auto preferRelative = Luau::startsWith(from.path, to.path) || Luau::startsWith(to.path, from.path) || fromParent == toParent;
                if (!preferRelative)
                    return {*bestAliasPath, SortText::AutoImportsAbsolute};
            }
        }
    }

    auto relativePath = to.lexicallyRelative(fromParent ? *fromParent : from);
    relativePath = removeSuffix(relativePath, ".luau");
    relativePath = removeSuffix(relativePath, ".lua");

    if (isInitLuauFile(from))
    {
        // Move the relative path up one directory
        if (!Luau::startsWith(relativePath, "../"))
            relativePath = "@self/" + relativePath;
        else
        {
            relativePath = removePrefix(relativePath, "../");
            if (!Luau::startsWith(relativePath, "../"))
                relativePath = "./" + relativePath;
        }
    }
    else
    {
        if (!Luau::startsWith(relativePath, "../"))
            relativePath = "./" + relativePath;
    }

    return {relativePath, SortText::AutoImports};
}

std::vector<StringRequireResult> computeAllStringRequires(const StringRequireAutoImporterContext& ctx)
{
    std::vector<StringRequireResult> result;
    size_t minimumLineNumber = computeMinimumLineNumberForRequire(*ctx.importsVisitor, ctx.hotCommentsLineNumber);

    auto fromUri = ctx.workspaceFolder->fileResolver.getUri(ctx.from);
    auto availableAliases = ctx.workspaceFolder->fileResolver.getConfig(ctx.from, ctx.workspaceFolder->limits).aliases;

    ScriptContext callerContext = getScriptContext(ctx, ctx.from);

    auto processModule = [&](const Luau::ModuleName& moduleName)
    {
        auto name = requireNameFromModuleName(moduleName);

        if (moduleName == ctx.from || ctx.importsVisitor->containsRequire(name))
            return;

        if (ctx.moduleFilter && !(*ctx.moduleFilter)(name))
            return;

        ScriptContext targetContext = getScriptContext(ctx, moduleName);
        if (callerContext == ScriptContext::Client && targetContext == ScriptContext::Server)
            return;
        if (callerContext == ScriptContext::Server && targetContext == ScriptContext::Client)
            return;

        auto uri = ctx.workspaceFolder->fileResolver.getUri(moduleName);
        if (ctx.workspaceFolder->isIgnoredFileForAutoImports(uri))
            return;

        std::string require;
        const char* sortText;
        if (ctx.requirePathComputer)
        {
            auto computed = (*ctx.requirePathComputer)(ctx.from, moduleName);
            if (!computed)
                return;
            std::tie(require, sortText) = *computed;
        }
        else
        {
            std::tie(require, sortText) = computeRequirePath(fromUri, uri, availableAliases, ctx.config->requireStyle);
        }

        size_t lineNumber = computeBestLineForRequire(*ctx.importsVisitor, *ctx.textDocument, require, minimumLineNumber);

        bool prependNewline = ctx.config->separateGroupsWithLine && ctx.importsVisitor->shouldPrependNewline(lineNumber);

        result.emplace_back(StringRequireResult{
            name,
            moduleName,
            require,
            createRequireTextEdit(name, '"' + require + '"', lineNumber, prependNewline),
            sortText,
        });
    };

    ctx.modules(processModule);

    return result;
}

void suggestStringRequires(const StringRequireAutoImporterContext& ctx, std::vector<lsp::CompletionItem>& items)
{
    auto availableStringRequires = computeAllStringRequires(ctx);
    for (const auto& [variableName, moduleName, requirePath, edit, sortText] : availableStringRequires)
        items.emplace_back(createSuggestRequire(variableName, {edit}, sortText, moduleName, requirePath));
}
} // namespace Luau::LanguageServer::AutoImports
