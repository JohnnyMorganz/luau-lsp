#include "Platform/LSPPlatform.hpp"

#include "LuauFileUtils.hpp"
#include "LSP/ClientConfiguration.hpp"
#include "LSP/Workspace.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "Platform/StringRequireSuggester.hpp"
#include "Platform/StringRequireAutoImporter.hpp"

#include "Luau/TimeTrace.h"
#include <memory>
#include <unordered_set>

LSPPlatform::LSPPlatform(WorkspaceFileResolver* fileResolver, WorkspaceFolder* workspaceFolder)
    : fileResolver(fileResolver)
    , workspaceFolder(workspaceFolder)
{
}

std::unique_ptr<LSPPlatform> LSPPlatform::getPlatform(
    const ClientConfiguration& config, WorkspaceFileResolver* fileResolver, WorkspaceFolder* workspaceFolder)
{
    if (config.types.roblox && config.platform.type == LSPPlatformConfig::Roblox)
        return std::make_unique<RobloxPlatform>(fileResolver, workspaceFolder);

    return std::make_unique<LSPPlatform>(fileResolver, workspaceFolder);
}

std::unique_ptr<Luau::RequireSuggester> LSPPlatform::getRequireSuggester()
{
    return std::make_unique<StringRequireSuggester>(workspaceFolder, fileResolver, this);
}

std::optional<Uri> LSPPlatform::resolveToRealPath(const Luau::ModuleName& name) const
{
    // Caution: if `isVirtualPath` returns true, then this would infinite loop
    // But, we want to use the default WorkspaceFileResolver::getUri() function in case that changes in the future
    return fileResolver->getUri(name);
}

std::optional<Luau::ModuleName> LSPPlatform::inferModuleNameFromUri(const Uri& uri) const
{
    return fileResolver->getModuleName(uri);
}

std::optional<std::string> LSPPlatform::computeNewRequirePath(
    const Luau::ModuleName& dependentModuleName,
    const Luau::ModuleName& newTargetModuleName,
    const Luau::AstNode* originalNode,
    const ClientConfiguration& config) const
{
    auto dependentUri = fileResolver->getUri(dependentModuleName);
    auto targetUri = fileResolver->getUri(newTargetModuleName);

    auto availableAliases = fileResolver->getConfig(dependentModuleName, workspaceFolder->limits).aliases;
    auto [newPath, _] = Luau::LanguageServer::AutoImports::computeRequirePath(
        dependentUri, targetUri, availableAliases, config.completion.imports.requireStyle);

    return "\"" + newPath + "\"";
}

std::optional<std::string> LSPPlatform::readSourceCode(const Luau::ModuleName& name, const Uri& path) const
{
    LUAU_TIMETRACE_SCOPE("LSPPlatform::readSourceCode", "LSP");
    if (auto textDocument = fileResolver->getTextDocumentFromModuleName(name))
        return textDocument->getText();

    if (path.extension() == ".lua" || path.extension() == ".luau")
        return Luau::FileUtils::readFile(path.fsPath());

    return std::nullopt;
}

Uri resolveAliasLocation(const Luau::Config::AliasInfo& aliasInfo)
{
    return Uri::file(aliasInfo.configLocation).resolvePath(resolvePath(aliasInfo.value));
}

std::optional<Uri> resolveAlias(const std::string& path, const Luau::Config& config, const Uri& from)
{
    if (path.size() < 1 || path[0] != '@')
        return std::nullopt;

    // To ignore the '@' alias prefix when processing the alias
    const size_t aliasStartPos = 1;

    // If a directory separator was found, the length of the alias is the
    // distance between the start of the alias and the separator. Otherwise,
    // the whole string after the alias symbol is the alias.
    size_t aliasLen = path.find_first_of("\\/");
    if (aliasLen != std::string::npos)
        aliasLen -= aliasStartPos;

    std::string potentialAlias = path.substr(aliasStartPos, aliasLen);

    // Not worth searching when potentialAlias cannot be an alias
    if (!Luau::isValidAlias(potentialAlias))
    {
        // TODO: report error: "@" + potentialAlias + " is not a valid alias");
        return std::nullopt;
    }

    // Luau aliases are case insensitive
    std::transform(potentialAlias.begin(), potentialAlias.end(), potentialAlias.begin(),
        [](unsigned char c)
        {
            return ('A' <= c && c <= 'Z') ? (c + ('a' - 'A')) : c;
        });

    Uri resolvedUri;
    if (auto aliasInfo = config.aliases.find(potentialAlias))
        resolvedUri = resolveAliasLocation(*aliasInfo);
    else if (potentialAlias == "self")
        resolvedUri = from;
    else
        // TODO: report error: "@" + potentialAlias + " is not a valid alias"
        return std::nullopt;

    auto remainder = path.substr(potentialAlias.size() + 1);

    // If remainder begins with a '/' character, we need to trim it off before it gets mistaken for an
    // absolute path
    remainder.erase(0, remainder.find_first_not_of("/\\"));

    if (remainder.empty())
        return resolvedUri;
    else
        return resolvedUri.resolvePath(remainder);
}

// DEPRECATED: Resolve the string using a directory alias if present
std::optional<Uri> resolveDirectoryAlias(
    const Uri& rootUri, const std::unordered_map<std::string, std::string>& directoryAliases, const std::string& str)
{
    for (const auto& [alias, directoryPath] : directoryAliases)
    {
        if (Luau::startsWith(str, alias))
        {
            std::string remainder = str.substr(alias.length());

            // If remainder begins with a '/' character, we need to trim it off before it gets mistaken for an
            // absolute path
            remainder.erase(0, remainder.find_first_not_of("/\\"));

            auto filePath = resolvePath(remainder.empty() ? directoryPath : Luau::FileUtils::joinPaths(directoryPath, remainder));
            if (Luau::FileUtils::isAbsolutePath(filePath))
                return Uri::file(filePath);
            else
                return rootUri.resolvePath(filePath);
        }
    }

    return std::nullopt;
}

std::optional<Luau::ModuleInfo> LSPPlatform::resolveStringRequire(
    const Luau::ModuleInfo* context, const std::string& requiredString, const Luau::TypeCheckLimits& limits)
{
    if (!context)
        return std::nullopt;

    auto contextPath = resolveToRealPath(context->name);
    if (!contextPath)
        return std::nullopt;

    auto baseUri = contextPath->parent();
    if (!baseUri)
        return std::nullopt;

    ClientConfiguration clientConfig;
    if (fileResolver->client)
        clientConfig = fileResolver->client->getConfiguration(fileResolver->rootUri);

    if (isInitLuauFile(*contextPath) && !clientConfig.require.useOriginalRequireByStringSemantics)
    {
        baseUri = baseUri->parent();
        if (!baseUri)
            return std::nullopt;
    }

    auto fileUri = baseUri->resolvePath(requiredString);

    auto luauConfig = fileResolver->getConfig(context->name, limits);
    if (auto aliasedPath = resolveAlias(requiredString, luauConfig, *contextPath->parent()))
    {
        fileUri = aliasedPath.value();
    }
    // DEPRECATED: Check for custom require overrides
    else if (fileResolver->client)
    {
        // Check file aliases
        if (auto it = clientConfig.require.fileAliases.find(requiredString); it != clientConfig.require.fileAliases.end())
        {
            fileUri = Uri::file(resolvePath(it->second));
        }
        // Check directory aliases
        else if (auto directoryAliasedPath = resolveDirectoryAlias(fileResolver->rootUri, clientConfig.require.directoryAliases, requiredString))
        {
            fileUri = *directoryAliasedPath;
        }
    }

    // Handle "init.luau" files in a directory
    if (fileUri.isDirectory())
        fileUri = fileUri.resolvePath("init");

    // Add file endings
    if (fileUri.extension() != ".luau" && fileUri.extension() != ".lua")
    {
        auto fileUriWithExtension = fileUri;
        fileUriWithExtension.path = fileUri.path + ".luau";
        if (!fileUriWithExtension.exists())
            // fall back to .lua if a module with .luau doesn't exist
            fileUri.path += ".lua";
        else
            fileUri.path = fileUriWithExtension.path;
    }

    return Luau::ModuleInfo{fileResolver->getModuleName(fileUri)};
}

std::optional<Luau::ModuleInfo> LSPPlatform::resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* node, const Luau::TypeCheckLimits& limits)
{
    // Handle require("path") for compatibility
    if (auto* expr = node->as<Luau::AstExprConstantString>())
    {
        std::string requiredString(expr->value.data, expr->value.size);
        return resolveStringRequire(context, requiredString, limits);
    }

    return std::nullopt;
}

std::optional<Luau::AutocompleteEntryMap> LSPPlatform::completionCallback(
    const std::string& tag, std::optional<const Luau::ExternType*> ctx, std::optional<std::string> contents, const Luau::ModuleName& moduleName)
{
    return std::nullopt;
}

void LSPPlatform::handleSuggestImports(const TextDocument& textDocument, const Luau::SourceModule& module, const ClientConfiguration& config,
    size_t hotCommentsLineNumber, bool completingTypeReferencePrefix, std::vector<lsp::CompletionItem>& items)
{
    if (!config.completion.imports.suggestRequires)
        return;

    LUAU_ASSERT(module.root);
    Luau::LanguageServer::AutoImports::FindImportsVisitor importsVisitor;
    importsVisitor.visit(module.root);

    Luau::LanguageServer::AutoImports::StringRequireAutoImporterContext ctx{
        module.name,
        Luau::NotNull(&textDocument),
        Luau::NotNull(&workspaceFolder->frontend),
        Luau::NotNull(workspaceFolder),
        Luau::NotNull(&config.completion.imports),
        hotCommentsLineNumber,
        Luau::NotNull(&importsVisitor),
    };

    return Luau::LanguageServer::AutoImports::suggestStringRequires(ctx, items);
}

void LSPPlatform::handleUnknownSymbolFix(const UnknownSymbolFixContext& ctx, const Luau::UnknownSymbol& unknownSymbol,
    const std::optional<lsp::Diagnostic>& diagnostic, std::vector<lsp::CodeAction>& result)
{
    if (unknownSymbol.context != Luau::UnknownSymbol::Binding)
        return;

    LUAU_ASSERT(ctx.sourceModule->root);
    Luau::LanguageServer::AutoImports::FindImportsVisitor importsVisitor;
    importsVisitor.visit(ctx.sourceModule->root);

    ClientConfiguration config = workspaceFolder->fileResolver.client->getConfiguration(workspaceFolder->rootUri);
    auto hotCommentsLineNumber = Luau::LanguageServer::AutoImports::computeHotCommentsLineNumber(*ctx.sourceModule);

    Luau::LanguageServer::AutoImports::StringRequireAutoImporterContext importCtx{
        ctx.sourceModule->name,
        Luau::NotNull(ctx.textDocument),
        Luau::NotNull(&ctx.workspaceFolder->frontend),
        ctx.workspaceFolder,
        Luau::NotNull(&config.completion.imports),
        hotCommentsLineNumber,
        Luau::NotNull(&importsVisitor),
        [&unknownSymbol](const std::string& requireName)
        {
            return requireName == unknownSymbol.name;
        },
    };

    const auto results = Luau::LanguageServer::AutoImports::computeAllStringRequires(importCtx);
    for (const auto& stringRequire : results)
    {
        lsp::CodeAction action;
        action.title = "Add require for '" + stringRequire.variableName + "' from \"" + stringRequire.requirePath + "\"";
        action.kind = lsp::CodeActionKind::QuickFix;
        action.isPreferred = results.size() == 1;

        if (diagnostic)
            action.diagnostics.push_back(*diagnostic);

        lsp::WorkspaceEdit workspaceEdit;
        workspaceEdit.changes.emplace(ctx.uri, std::vector{stringRequire.edit});
        action.edit = workspaceEdit;

        result.push_back(action);
    }
}

std::vector<lsp::TextEdit> LSPPlatform::computeAddAllMissingImportsEdits(
    const UnknownSymbolFixContext& ctx, const std::vector<Luau::TypeError>& errors)
{
    std::vector<lsp::TextEdit> edits;

    Luau::LanguageServer::AutoImports::FindImportsVisitor importsVisitor;
    importsVisitor.visit(ctx.sourceModule->root);

    ClientConfiguration config = workspaceFolder->fileResolver.client->getConfiguration(workspaceFolder->rootUri);
    auto hotCommentsLineNumber = Luau::LanguageServer::AutoImports::computeHotCommentsLineNumber(*ctx.sourceModule);

    std::vector<std::string> unknownSymbols;
    std::unordered_set<std::string> addedRequires;

    for (const auto& error : errors)
    {
        const auto* unknownSymbol = Luau::get_if<Luau::UnknownSymbol>(&error.data);
        if (!unknownSymbol || unknownSymbol->context != Luau::UnknownSymbol::Binding)
            continue;

        unknownSymbols.emplace_back(unknownSymbol->name);
    }

    Luau::LanguageServer::AutoImports::StringRequireAutoImporterContext importCtx{
        ctx.sourceModule->name,
        Luau::NotNull(ctx.textDocument),
        Luau::NotNull(&ctx.workspaceFolder->frontend),
        ctx.workspaceFolder,
        Luau::NotNull(&config.completion.imports),
        hotCommentsLineNumber,
        Luau::NotNull(&importsVisitor),
        [&unknownSymbols](const std::string& requireName)
        {
            return contains(unknownSymbols, requireName);
        },
    };

    const auto results = computeAllStringRequires(importCtx);
    for (const auto& stringRequire : results)
    {
        if (addedRequires.find(stringRequire.variableName) != addedRequires.end())
            continue;

        edits.push_back(stringRequire.edit);
        addedRequires.insert(stringRequire.variableName);
    }

    std::sort(edits.begin(), edits.end(),
        [](const lsp::TextEdit& a, const lsp::TextEdit& b)
        {
            return a.range.start.line == b.range.start.line ? a.newText < b.newText : a.range.start.line < b.range.start.line;
        });

    return edits;
}
