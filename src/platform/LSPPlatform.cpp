#include "Platform/LSPPlatform.hpp"

#include "LuauFileUtils.hpp"
#include "LSP/ClientConfiguration.hpp"
#include "LSP/Workspace.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "Platform/StringRequireSuggester.hpp"
#include "Platform/StringRequireAutoImporter.hpp"

#include "Luau/TimeTrace.h"
#include <memory>
#include <set>

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

std::optional<Luau::ModuleInfo> LSPPlatform::resolveStringRequire(const Luau::ModuleInfo* context, const std::string& requiredString, const Luau::TypeCheckLimits& limits)
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

namespace
{
// Find all modules that can be required with a variable name matching the given name
std::vector<std::pair<Luau::ModuleName, std::string>> findModulesForName(
    const std::string& name, const Luau::ModuleName& fromModule, const Luau::Frontend& frontend, const WorkspaceFolder& workspaceFolder)
{
    std::vector<std::pair<Luau::ModuleName, std::string>> matches;

    for (const auto& [moduleName, sourceNode] : frontend.sourceNodes)
    {
        if (moduleName == fromModule)
            continue;

        auto requireName = Luau::LanguageServer::AutoImports::requireNameFromModuleName(moduleName);
        if (requireName == name)
        {
            auto uri = workspaceFolder.fileResolver.getUri(moduleName);
            if (workspaceFolder.isIgnoredFileForAutoImports(uri))
                continue;

            matches.emplace_back(moduleName, requireName);
        }
    }
    return matches;
}
} // namespace

void LSPPlatform::handleUnknownSymbolFix(const UnknownSymbolFixContext& ctx, const Luau::UnknownSymbol& unknownSymbol,
    const std::optional<lsp::Diagnostic>& diagnostic, std::vector<lsp::CodeAction>& result)
{
    // Only handle binding context (not type references)
    if (unknownSymbol.context != Luau::UnknownSymbol::Binding)
        return;

    // Find all modules that match this name
    auto moduleMatches = findModulesForName(unknownSymbol.name, ctx.sourceModule->name, *ctx.frontend, *workspaceFolder);
    if (moduleMatches.empty())
        return;

    // Find existing imports to determine best insertion line
    Luau::LanguageServer::AutoImports::FindImportsVisitor importsVisitor;
    importsVisitor.visit(ctx.sourceModule->root);

    // Compute the require path
    auto fromUri = workspaceFolder->fileResolver.getUri(ctx.sourceModule->name);
    auto availableAliases = workspaceFolder->fileResolver.getConfig(ctx.sourceModule->name, workspaceFolder->limits).aliases;

    ClientConfiguration config;
    if (workspaceFolder->fileResolver.client)
        config = workspaceFolder->fileResolver.client->getConfiguration(workspaceFolder->rootUri);

    // Generate a code action for each matching module
    bool isFirst = true;
    for (const auto& [moduleName, requireName] : moduleMatches)
    {
        // Check if this module is already imported
        if (importsVisitor.containsRequire(requireName))
            continue;

        auto toUri = workspaceFolder->fileResolver.getUri(moduleName);

        auto [requirePath, sortText] =
            Luau::LanguageServer::AutoImports::computeRequirePath(fromUri, toUri, availableAliases, config.completion.imports.requireStyle);

        size_t minimumLineNumber =
            Luau::LanguageServer::AutoImports::computeMinimumLineNumberForRequire(importsVisitor, ctx.hotCommentsLineNumber);
        size_t lineNumber =
            Luau::LanguageServer::AutoImports::computeBestLineForRequire(importsVisitor, *ctx.textDocument, requirePath, minimumLineNumber);

        bool prependNewline = config.completion.imports.separateGroupsWithLine && importsVisitor.shouldPrependNewline(lineNumber);
        auto requireEdit =
            Luau::LanguageServer::AutoImports::createRequireTextEdit(requireName, '"' + requirePath + '"', lineNumber, prependNewline);

        lsp::CodeAction action;
        action.title = "Add require for '" + requireName + "' from \"" + requirePath + "\"";
        action.kind = lsp::CodeActionKind::QuickFix;
        action.isPreferred = isFirst; // Only the first match is preferred

        if (diagnostic)
            action.diagnostics.push_back(*diagnostic);

        lsp::WorkspaceEdit workspaceEdit;
        workspaceEdit.changes.emplace(ctx.uri, std::vector{requireEdit});
        action.edit = workspaceEdit;

        result.push_back(action);
        isFirst = false;
    }
}

std::vector<lsp::TextEdit> LSPPlatform::computeAddAllMissingImportsEdits(
    const UnknownSymbolFixContext& ctx, const std::vector<Luau::TypeError>& errors)
{
    std::vector<lsp::TextEdit> edits;
    std::set<std::string> addedRequires; // Track which requires we've already added

    // Find existing imports
    Luau::LanguageServer::AutoImports::FindImportsVisitor importsVisitor;
    importsVisitor.visit(ctx.sourceModule->root);

    auto fromUri = workspaceFolder->fileResolver.getUri(ctx.sourceModule->name);
    auto availableAliases = workspaceFolder->fileResolver.getConfig(ctx.sourceModule->name, workspaceFolder->limits).aliases;

    ClientConfiguration config;
    if (workspaceFolder->fileResolver.client)
        config = workspaceFolder->fileResolver.client->getConfiguration(workspaceFolder->rootUri);

    size_t minimumLineNumber =
        Luau::LanguageServer::AutoImports::computeMinimumLineNumberForRequire(importsVisitor, ctx.hotCommentsLineNumber);

    for (const auto& error : errors)
    {
        const auto* unknownSymbol = Luau::get_if<Luau::UnknownSymbol>(&error.data);
        if (!unknownSymbol || unknownSymbol->context != Luau::UnknownSymbol::Binding)
            continue;

        // Skip if we've already added a require for this name
        if (addedRequires.count(unknownSymbol->name))
            continue;

        // Skip if already imported
        if (importsVisitor.containsRequire(unknownSymbol->name))
            continue;

        // Find matching modules
        auto moduleMatches = findModulesForName(unknownSymbol->name, ctx.sourceModule->name, *ctx.frontend, *workspaceFolder);
        if (moduleMatches.empty())
            continue;

        // Use the first matching module
        const auto& [moduleName, requireName] = moduleMatches[0];
        auto toUri = workspaceFolder->fileResolver.getUri(moduleName);

        auto [requirePath, sortText] =
            Luau::LanguageServer::AutoImports::computeRequirePath(fromUri, toUri, availableAliases, config.completion.imports.requireStyle);

        size_t lineNumber =
            Luau::LanguageServer::AutoImports::computeBestLineForRequire(importsVisitor, *ctx.textDocument, requirePath, minimumLineNumber);

        bool prependNewline = config.completion.imports.separateGroupsWithLine && importsVisitor.shouldPrependNewline(lineNumber);
        auto requireEdit =
            Luau::LanguageServer::AutoImports::createRequireTextEdit(requireName, '"' + requirePath + '"', lineNumber, prependNewline);

        edits.push_back(requireEdit);
        addedRequires.insert(requireName);
    }

    return edits;
}
