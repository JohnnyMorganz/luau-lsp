#pragma once
#include <unordered_map>
#include <string>
#include <filesystem>
#include "argparse/argparse.hpp"

#include "Analyze/CliClient.hpp"

std::vector<std::filesystem::path> getFilesToAnalyze(const std::vector<std::string>& paths, const std::vector<std::string>& ignoreGlobPatterns);
void applySettings(const std::string& settingsContents, CliClient& client, std::vector<std::string>& ignoreGlobPatterns,
    std::vector<std::filesystem::path>& definitionsPaths);
int startAnalyze(const argparse::ArgumentParser& program);
