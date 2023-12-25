#pragma once
#include <unordered_map>
#include <string>
#include <filesystem>
#include "argparse/argparse.hpp"

void registerFastFlags(std::unordered_map<std::string, std::string>& fastFlags);
std::unordered_map<std::string, std::filesystem::path> processDefinitionsFilePaths(const argparse::ArgumentParser& program);
int startAnalyze(const argparse::ArgumentParser& program);
