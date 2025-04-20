#include "Platform/LSPPlatform.hpp"

#include "LSP/ClientConfiguration.hpp"
#include "LSP/Workspace.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "Platform/StringRequireSuggester.hpp"
#include "Platform/StringRequireAutoImporter.hpp"

#include <memory>

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
    return std::make_unique<StringRequireSuggester>(fileResolver, this);
}

std::optional<std::string> LSPPlatform::readSourceCode(const Luau::ModuleName& name, const std::filesystem::path& path) const
{
    if (auto textDocument = fileResolver->getTextDocumentFromModuleName(name))
        return textDocument->getText();

    if (path.extension() == ".lua" || path.extension() == ".luau")
        return readFile(path);

    return std::nullopt;
}

std::filesystem::path resolveAliasLocation(const Luau::Config::AliasInfo& aliasInfo)
{
    return std::filesystem::path(aliasInfo.configLocation) / resolvePath(aliasInfo.value);
}

std::optional<std::filesystem::path> resolveAlias(const std::string& path, const Luau::Config& config)
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

    if (auto aliasInfo = config.aliases.find(potentialAlias))
    {
        auto remainder = path.substr(potentialAlias.size() + 1);

        // If remainder begins with a '/' character, we need to trim it off before it gets mistaken for an
        // absolute path
        remainder.erase(0, remainder.find_first_not_of("/\\"));

        auto resolvedPath = resolveAliasLocation(*aliasInfo);
        if (remainder.empty())
            return resolvedPath;
        else
            return resolvedPath / remainder;
    }

    // TODO: report error: "@" + potentialAlias + " is not a valid alias"
    return std::nullopt;
}

// DEPRECATED: Resolve the string using a directory alias if present
std::optional<std::filesystem::path> resolveDirectoryAlias(
    const std::filesystem::path& rootPath, const std::unordered_map<std::string, std::string>& directoryAliases, const std::string& str)
{
    for (const auto& [alias, path] : directoryAliases)
    {
        if (Luau::startsWith(str, alias))
        {
            std::filesystem::path directoryPath = path;
            std::string remainder = str.substr(alias.length());

            // If remainder begins with a '/' character, we need to trim it off before it gets mistaken for an
            // absolute path
            remainder.erase(0, remainder.find_first_not_of("/\\"));

            auto filePath = resolvePath(remainder.empty() ? directoryPath : directoryPath / remainder);
            if (!filePath.is_absolute())
                filePath = rootPath / filePath;

            return filePath;
        }
    }

    return std::nullopt;
}

std::optional<Luau::ModuleInfo> LSPPlatform::resolveStringRequire(const Luau::ModuleInfo* context, const std::string& requiredString)
{
    if (!context)
        return std::nullopt;

    auto contextPath = resolveToRealPath(context->name);
    if (!contextPath)
        return std::nullopt;

    std::filesystem::path basePath = contextPath->parent_path();
    auto filePath = basePath / requiredString;

    auto luauConfig = fileResolver->getConfig(context->name);
    if (auto aliasedPath = resolveAlias(requiredString, luauConfig))
    {
        filePath = aliasedPath.value();
    }
    // DEPRECATED: Check for custom require overrides
    else if (fileResolver->client)
    {
        auto config = fileResolver->client->getConfiguration(fileResolver->rootUri);

        // Check file aliases
        if (auto it = config.require.fileAliases.find(requiredString); it != config.require.fileAliases.end())
        {
            filePath = resolvePath(it->second);
        }
        // Check directory aliases
        else if (auto directoryAliasedPath = resolveDirectoryAlias(fileResolver->rootUri.fsPath(), config.require.directoryAliases, requiredString))
        {
            filePath = directoryAliasedPath.value();
        }
    }

    std::error_code ec;
    filePath = std::filesystem::weakly_canonical(filePath, ec);

    // Handle "init.luau" files in a directory
    if (std::filesystem::is_directory(filePath, ec))
    {
        filePath /= "init";
    }

    // Add file endings
    if (filePath.extension() != ".luau" && filePath.extension() != ".lua")
    {
        auto fullFilePath = filePath.string() + ".luau";
        if (!std::filesystem::exists(fullFilePath))
            // fall back to .lua if a module with .luau doesn't exist
            filePath = filePath.string() + ".lua";
        else
            filePath = fullFilePath;
    }

    // URI-ify the file path so that its normalised (in particular, the drive letter)
    auto uri = Uri::file(filePath);

    return Luau::ModuleInfo{fileResolver->getModuleName(uri)};
}

std::optional<Luau::ModuleInfo> LSPPlatform::resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* node)
{
    // Handle require("path") for compatibility
    if (auto* expr = node->as<Luau::AstExprConstantString>())
    {
        std::string requiredString(expr->value.data, expr->value.size);
        return resolveStringRequire(context, requiredString);
    }

    return std::nullopt;
}

std::optional<Luau::AutocompleteEntryMap> LSPPlatform::completionCallback(
    const std::string& tag, std::optional<const Luau::ClassType*> ctx, std::optional<std::string> contents, const Luau::ModuleName& moduleName)
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