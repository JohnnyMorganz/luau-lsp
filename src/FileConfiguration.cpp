#include "LSP/FileConfiguration.hpp"
#include "Luau/LuauConfig.h"
#include "LuauFileUtils.hpp"

static std::string resolveConfigPath(const std::string& path, const std::string& configDir)
{
    if (Luau::FileUtils::isAbsolutePath(path))
        return Luau::FileUtils::normalizePath(path);

    return Luau::FileUtils::normalizePath(Luau::FileUtils::joinPaths(configDir, path));
}


static std::optional<std::string> parsePlatformConfig(const Luau::ConfigTable& platformTable, FilePlatformConfiguration& result)
{
    if (const Luau::ConfigValue* typeValue = platformTable.find("type"))
    {
        const std::string* typeStr = typeValue->get_if<std::string>();
        if (!typeStr)
            return "configuration value for key \"lsp.platform.type\" must be a string";

        if (*typeStr == "roblox")
            result.type = LSPPlatformConfig::Roblox;
        else if (*typeStr == "standard")
            result.type = LSPPlatformConfig::Standard;
        else
            return "configuration value for key \"lsp.platform.type\" must be \"standard\" or \"roblox\"";
    }

    return std::nullopt;
}

static std::optional<std::string> parseSourcemapConfig(
    const Luau::ConfigTable& sourcemapTable,
    FileSourcemapConfiguration& result,
    const std::string& configDir
)
{
    if (const Luau::ConfigValue* value = sourcemapTable.find("rojoProjectFile"))
    {
        const std::string* str = value->get_if<std::string>();
        if (!str)
            return "configuration value for key \"lsp.sourcemap.rojoProjectFile\" must be a string";
        result.rojoProjectFile = resolveConfigPath(*str, configDir);
    }

    if (const Luau::ConfigValue* value = sourcemapTable.find("sourcemapFile"))
    {
        const std::string* str = value->get_if<std::string>();
        if (!str)
            return "configuration value for key \"lsp.sourcemap.sourcemapFile\" must be a string";
        result.sourcemapFile = resolveConfigPath(*str, configDir);
    }

    return std::nullopt;
}

static std::optional<std::string> parseTypesConfig(
    const Luau::ConfigTable& typesTable,
    FileTypesConfiguration& result,
    const std::string& configDir
)
{
    if (const Luau::ConfigValue* defFilesValue = typesTable.find("definitionFiles"))
    {
        const Luau::ConfigTable* defFilesTable = defFilesValue->get_if<Luau::ConfigTable>();
        if (!defFilesTable)
            return "configuration value for key \"lsp.types.definitionFiles\" must be a table";

        std::unordered_map<std::string, std::string> definitionFiles;
        for (const auto& [k, v] : *defFilesTable)
        {
            const std::string* key = k.get_if<std::string>();
            if (!key)
                return "configuration keys in \"lsp.types.definitionFiles\" table must be strings";

            const std::string* val = v.get_if<std::string>();
            if (!val)
                return "configuration values in \"lsp.types.definitionFiles\" table must be strings";

            definitionFiles[*key] = resolveConfigPath(*val, configDir);
        }
        result.definitionFiles = std::move(definitionFiles);
    }

    if (const Luau::ConfigValue* globalsValue = typesTable.find("disabledGlobals"))
    {
        const Luau::ConfigTable* globalsTable = globalsValue->get_if<Luau::ConfigTable>();
        if (!globalsTable)
            return "configuration value for key \"lsp.types.disabledGlobals\" must be an array";

        std::vector<std::string> disabledGlobals;
        disabledGlobals.resize(globalsTable->size());

        for (const auto& [k, v] : *globalsTable)
        {
            const double* index = k.get_if<double>();
            if (!index)
                return "configuration array \"lsp.types.disabledGlobals\" must only have numeric keys";

            const size_t idx = static_cast<size_t>(*index);
            if (idx < 1 || globalsTable->size() < idx)
                return "configuration array \"lsp.types.disabledGlobals\" contains invalid numeric key";

            const std::string* val = v.get_if<std::string>();
            if (!val)
                return "configuration values in \"lsp.types.disabledGlobals\" must be strings";

            disabledGlobals[idx - 1] = *val;
        }
        result.disabledGlobals = std::move(disabledGlobals);
    }

    return std::nullopt;
}

bool FileConfiguration::hasAnyValue() const
{
    return platform.type.has_value() || sourcemap.rojoProjectFile.has_value() || sourcemap.sourcemapFile.has_value() ||
           types.definitionFiles.has_value() || types.disabledGlobals.has_value();
}

std::optional<std::string> extractLspConfigFromTable(
    const Luau::ConfigTable& configTable,
    FileConfiguration& result,
    const std::string& configDir
)
{
    const Luau::ConfigValue* lspValue = configTable.find("lsp");
    if (!lspValue)
        return std::nullopt;

    const Luau::ConfigTable* lspTable = lspValue->get_if<Luau::ConfigTable>();
    if (!lspTable)
        return "configuration value for key \"lsp\" must be a table";

    if (const Luau::ConfigValue* platformValue = lspTable->find("platform"))
    {
        const Luau::ConfigTable* platformTable = platformValue->get_if<Luau::ConfigTable>();
        if (!platformTable)
            return "configuration value for key \"lsp.platform\" must be a table";
        if (auto error = parsePlatformConfig(*platformTable, result.platform))
            return error;
    }

    if (const Luau::ConfigValue* sourcemapValue = lspTable->find("sourcemap"))
    {
        const Luau::ConfigTable* sourcemapTable = sourcemapValue->get_if<Luau::ConfigTable>();
        if (!sourcemapTable)
            return "configuration value for key \"lsp.sourcemap\" must be a table";
        if (auto error = parseSourcemapConfig(*sourcemapTable, result.sourcemap, configDir))
            return error;
    }

    if (const Luau::ConfigValue* typesValue = lspTable->find("types"))
    {
        const Luau::ConfigTable* typesTable = typesValue->get_if<Luau::ConfigTable>();
        if (!typesTable)
            return "configuration value for key \"lsp.types\" must be a table";
        if (auto error = parseTypesConfig(*typesTable, result.types, configDir))
            return error;
    }

    return std::nullopt;
}

ClientConfiguration mergeConfigurations(
    const ClientConfiguration& editorConfig,
    const FileConfiguration& fileConfig
)
{
    ClientConfiguration result = editorConfig;

    if (fileConfig.platform.type)
        result.platform.type = *fileConfig.platform.type;
    if (fileConfig.sourcemap.rojoProjectFile)
        result.sourcemap.rojoProjectFile = *fileConfig.sourcemap.rojoProjectFile;
    if (fileConfig.sourcemap.sourcemapFile)
        result.sourcemap.sourcemapFile = *fileConfig.sourcemap.sourcemapFile;
    if (fileConfig.types.definitionFiles)
        result.types.definitionFiles = *fileConfig.types.definitionFiles;
    if (fileConfig.types.disabledGlobals)
        result.types.disabledGlobals = *fileConfig.types.disabledGlobals;

    return result;
}
