#pragma once

#include <optional>
#include <string>
#include <vector>

#include "argparse/argparse.hpp"

#include "Analyze/CliClient.hpp"

struct WorkspaceFileResolver;
class WorkspaceFolder;

struct ConfigFileData
{
    std::vector<std::string> definitions;   // @name=path entries or glob patterns
    std::vector<std::string> definitionsDir; // directory paths for recursive definition loading
    std::vector<std::string> docs;          // documentation database paths
    std::string platform;                   // "standard" or "roblox"
    std::string baseLuaurc;                 // path to .luaurc base config
};

// Load config file from explicit path, or auto-discover from project root.
// Auto-discovery walks from project root down to CWD, collecting and merging
// all luau-lsp-settings.json files found. Closer configs override/supplement.
// Returns std::nullopt if no config file found (not an error).
std::optional<ConfigFileData> loadConfigFile(const std::optional<std::string>& configPath);

std::unordered_map<std::string, std::string> processDefinitionsFilePaths(
    const argparse::ArgumentParser& program, const std::optional<ConfigFileData>& configData = std::nullopt);

enum class ReportFormat
{
    Default,
    Luacheck,
    Gnu,
};

struct FilePathInformation
{
    Uri uri;
    std::string relativePath;
};

FilePathInformation getFilePath(const WorkspaceFileResolver* fileResolver, const std::string& moduleName);

bool analyzeFile(WorkspaceFolder& workspace, const std::string& path, ReportFormat format, bool annotate);
std::vector<std::string> getFilesToAnalyze(const std::vector<std::string>& paths, WorkspaceFolder* workspace = nullptr);
void applySettings(const std::string& settingsContents, CliClient& client);
int startAnalyze(const argparse::ArgumentParser& program);
