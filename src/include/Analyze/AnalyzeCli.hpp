#pragma once

#include <string>
#include <vector>

#include "argparse/argparse.hpp"

#include "Analyze/CliClient.hpp"

struct WorkspaceFileResolver;
class WorkspaceFolder;

std::unordered_map<std::string, std::string> processDefinitionsFilePaths(const argparse::ArgumentParser& program);

struct FilePathInformation
{
    Uri uri;
    std::string relativePath;
};

FilePathInformation getFilePath(const WorkspaceFileResolver* fileResolver, const std::string& moduleName);

std::vector<std::string> getFilesToAnalyze(const std::vector<std::string>& paths, WorkspaceFolder* workspace = nullptr);
void applySettings(const std::string& settingsContents, CliClient& client);
int startAnalyze(const argparse::ArgumentParser& program);
