#pragma once
#include <unordered_map>
#include <string>
#include <filesystem>
#include "argparse/argparse.hpp"

std::vector<std::filesystem::path> getFilesToAnalyze(const std::vector<std::string>& paths, const std::vector<std::string>& ignoreGlobPatterns);
std::unordered_map<std::string, std::filesystem::path> processDefinitionsFilePaths(const argparse::ArgumentParser& program);
int startAnalyze(const argparse::ArgumentParser& program);
