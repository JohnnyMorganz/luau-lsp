#pragma once
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include "LSP/ClientConfiguration.hpp"

namespace Luau
{
struct ConfigTable;
}

struct FilePlatformConfiguration
{
    std::optional<LSPPlatformConfig> type;
};

struct FileSourcemapConfiguration
{
    std::optional<std::string> rojoProjectFile;
    std::optional<std::string> sourcemapFile;
};

struct FileTypesConfiguration
{
    std::optional<std::unordered_map<std::string, std::string>> definitionFiles;
    std::optional<std::vector<std::string>> disabledGlobals;
};

struct FileConfiguration
{
    FilePlatformConfiguration platform;
    FileSourcemapConfiguration sourcemap;
    FileTypesConfiguration types;

    bool hasAnyValue() const;
};

/// Extract the `lsp` section from a ConfigTable returned by Luau::extractConfig().
/// Populates `result`. Returns an error string on parse failure, nullopt on success.
/// If no `lsp` key exists, returns nullopt and leaves `result` unchanged.
/// Relative paths in the config are resolved against `configDir`.
std::optional<std::string> extractLspConfigFromTable(
    const Luau::ConfigTable& configTable,
    FileConfiguration& result,
    const std::string& configDir
);

/// Merge file config over editor config. File config fields that have a value override
/// the corresponding editor config fields. Returns a new merged ClientConfiguration.
ClientConfiguration mergeConfigurations(
    const ClientConfiguration& editorConfig,
    const FileConfiguration& fileConfig
);
