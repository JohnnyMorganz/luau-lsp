#pragma once
#include <optional>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

std::optional<std::string> getParentPath(const std::string& path);
std::optional<std::string> getAncestorPath(const std::string& path, const std::string& ancestorName);
std::string codeBlock(std::string language, std::string code);
std::optional<std::string> readFile(const std::filesystem::path& filePath);
std::string trim_start(std::string& str);
std::string trim_end(std::string& str);
std::string trim(std::string& str);
std::string& toLower(std::string& str);
bool endsWith(const std::string_view& str, const std::string_view& suffix);

template<class K, class V>
inline bool contains(const std::unordered_map<K, V>& map, const K& value)
{
    return map.find(value) != map.end();
}