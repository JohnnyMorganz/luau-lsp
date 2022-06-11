#pragma once
#include <optional>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>

std::optional<std::string> getParentPath(const std::string& path);
std::optional<std::string> getAncestorPath(const std::string& path, const std::string& ancestorName);
std::string codeBlock(std::string language, std::string code);
std::optional<std::string> readFile(const std::filesystem::path& filePath);
void trim_start(std::string& str);
void trim_end(std::string& str);
void trim(std::string& str);
std::string& toLower(std::string& str);