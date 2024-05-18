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

// Resolve the string using a directory alias if present
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

/// Returns the base path to use in a string require.
/// This depends on user configuration, whether requires are taken relative to file or workspace root, defaulting to the latter
std::filesystem::path LSPPlatform::getRequireBasePath(std::optional<Luau::ModuleName> fileModuleName) const
{
    if (!fileResolver->client)
        return fileResolver->rootUri.fsPath();

    auto config = fileResolver->client->getConfiguration(fileResolver->rootUri);
    switch (config.require.mode)
    {
    case RequireModeConfig::RelativeToWorkspaceRoot:
        return fileResolver->rootUri.fsPath();
    case RequireModeConfig::RelativeToFile:
    {
        if (fileModuleName.has_value())
        {
            auto filePath = resolveToRealPath(*fileModuleName);
            if (filePath)
                return filePath->parent_path();
            else
                return fileResolver->rootUri.fsPath();
        }
        else
        {
            return fileResolver->rootUri.fsPath();
        }
    }
    }

    return fileResolver->rootUri.fsPath();
}

std::optional<Luau::ModuleInfo> LSPPlatform::resolveStringRequire(const Luau::ModuleInfo* context, const std::string& requiredString)
{
    std::filesystem::path basePath = getRequireBasePath(context ? std::optional(context->name) : std::nullopt);
    auto filePath = basePath / requiredString;

    // Check for custom require overrides
    if (fileResolver->client)
    {
        auto config = fileResolver->client->getConfiguration(fileResolver->rootUri);

        // Check file aliases
        if (auto it = config.require.fileAliases.find(requiredString); it != config.require.fileAliases.end())
        {
            filePath = resolvePath(it->second);
        }
        // Check directory aliases
        else if (auto aliasedPath = resolveDirectoryAlias(fileResolver->rootUri.fsPath(), config.require.directoryAliases, requiredString))
        {
            filePath = aliasedPath.value();
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

        // Populate with custom file aliases
        for (const auto& [aliasName, _] : config.require.fileAliases)
        {
            Luau::AutocompleteEntry entry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType, false, false,
                Luau::TypeCorrectKind::Correct};
            entry.tags.push_back("File");
            entry.tags.push_back("Alias");
            result.insert_or_assign(aliasName, entry);
        }

        // Populate with custom directory aliases, if we are at the start of a string require
        if (contentsString == "")
        {
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
        std::filesystem::path currentDirectory =
            resolveDirectoryAlias(workspaceFolder->rootUri.fsPath(), config.require.directoryAliases, contentsString)
                .value_or(getRequireBasePath(moduleName).append(contentsString));

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
