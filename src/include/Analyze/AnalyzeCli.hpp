#pragma once

#include <string>
#include <vector>

#include "argparse/argparse.hpp"

#include "Analyze/CliClient.hpp"

std::vector<std::string> getFilesToAnalyze(const std::vector<std::string>& paths, const std::vector<std::string>& ignoreGlobPatterns);
void applySettings(
    const std::string& settingsContents, CliClient& client, std::vector<std::string>& ignoreGlobPatterns, std::vector<std::string>& definitionsPaths);
int startAnalyze(const argparse::ArgumentParser& program);
