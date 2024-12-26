#pragma once
#include <unordered_map>
#include <string>
#include <filesystem>
#include "argparse/argparse.hpp"

std::vector<std::filesystem::path> getFilesToAnalyze(const std::vector<std::string>& paths, const std::vector<std::string>& ignoreGlobPatterns);
int startAnalyze(const argparse::ArgumentParser& program);
