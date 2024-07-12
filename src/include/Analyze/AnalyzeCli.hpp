#pragma once
#include <unordered_map>
#include <filesystem>
#include <string>
#include "argparse/argparse.hpp"

std::unordered_map<std::string, std::filesystem::path> processDefinitionsFilePaths(const argparse::ArgumentParser& program);
int startAnalyze(const argparse::ArgumentParser& program);
