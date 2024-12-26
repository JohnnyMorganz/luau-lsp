#include "Platform/LSPPlatform.hpp"

#include "LSP/ClientConfiguration.hpp"
#include "LSP/Workspace.hpp"
#include "Platform/RobloxPlatform.hpp"

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

std::optional<std::string> LSPPlatform::readSourceCode(const Luau::ModuleName& name, const std::filesystem::path& path) const
{
    if (auto textDocument = fileResolver->getTextDocumentFromModuleName(name))
        return textDocument->getText();

    if (path.extension() == ".lua" || path.extension() == ".luau")
        return readFile(path);

    return std::nullopt;
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

        auto resolvedPath = std::filesystem::path(aliasInfo->configLocation) / resolvePath(aliasInfo->value);
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
    auto filePath = workspaceFolder->rootUri.fsPath() / basePath / requiredString;

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

    filePath = normalizePath(filePath.generic_string());

    // Handle "init.luau" files in a directory
    std::error_code ec;
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
    auto uri = Uri::parse(Uri::file(filePath).toString());

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
    if (tag == "Require")
    {
        if (!contents.has_value())
            return std::nullopt;

        auto config = workspaceFolder->client->getConfiguration(workspaceFolder->rootUri);
        auto luauConfig = fileResolver->getConfig(moduleName);

        Luau::AutocompleteEntryMap result;

        // Include any files in the directory
        auto contentsString = contents.value();

        // We should strip any trailing values until a `/` is found in case autocomplete
        // is triggered half-way through.
        // E.g., for "Contents/Test|", we should only consider up to "Contents/" to find all files
        // For "Mod|", we should only consider an empty string ""
        auto separator = contentsString.find_last_of("/\\");
        if (separator == std::string::npos)
            contentsString = "";
        else
            contentsString = contentsString.substr(0, separator + 1);

        // Populate with custom aliases, if we are at the start of a string require
        if (contentsString.empty())
        {
            for (const auto& [_, aliasInfo] : luauConfig.aliases)
            {
                Luau::AutocompleteEntry entry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType, false, false,
                    Luau::TypeCorrectKind::Correct};
                entry.tags.push_back("Alias");
                result.insert_or_assign("@" + aliasInfo.originalCase, entry);
            }
            // DEPRECATED
            for (const auto& [aliasName, _] : config.require.fileAliases)
            {
                Luau::AutocompleteEntry entry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType, false, false,
                    Luau::TypeCorrectKind::Correct};
                entry.tags.push_back("File");
                entry.tags.push_back("Alias");
                result.insert_or_assign(aliasName, entry);
            }
            // DEPRECATED
            for (const auto& [aliasName, _] : config.require.directoryAliases)
            {
                Luau::AutocompleteEntry entry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType, false, false,
                    Luau::TypeCorrectKind::Correct};
                entry.tags.push_back("Directory");
                entry.tags.push_back("Alias");
                result.insert_or_assign(aliasName, entry);
            }
        }

        // Check if it starts with a directory alias, otherwise resolve with require base path
        std::filesystem::path currentDirectory;
        if (auto luaurcAlias = resolveAlias(contentsString, luauConfig))
            currentDirectory = luaurcAlias.value();
        else if (auto DEPRECATED_directoryAlias =
                     resolveDirectoryAlias(workspaceFolder->rootUri.fsPath(), config.require.directoryAliases, contentsString))
            currentDirectory = DEPRECATED_directoryAlias.value();
        else if (auto realPath = resolveToRealPath(moduleName); realPath && realPath->has_parent_path())
            currentDirectory = realPath->parent_path().append(contentsString);
        else
            // TODO: this is a weird fallback, maybe we should indicate an error somewhere
            currentDirectory = workspaceFolder->rootUri.fsPath().append(contentsString);

        try
        {
            for (const auto& dir_entry : std::filesystem::directory_iterator(currentDirectory))
            {
                if (dir_entry.is_regular_file() || dir_entry.is_directory())
                {
                    std::string fileName = dir_entry.path().filename().generic_string();
                    Luau::AutocompleteEntry entry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType, false,
                        false, Luau::TypeCorrectKind::Correct};
                    entry.tags.push_back(dir_entry.is_directory() ? "Directory" : "File");
                    result.insert_or_assign(fileName, entry);
                }
            }

            // Add in ".." support
            if (currentDirectory.has_parent_path())
            {
                Luau::AutocompleteEntry dotdotEntry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType, false,
                    false, Luau::TypeCorrectKind::Correct};
                dotdotEntry.tags.push_back("Directory");
                result.insert_or_assign("..", dotdotEntry);
            }
        }
        catch (std::exception&)
        {
        }

        return result;
    }

    return std::nullopt;
}
