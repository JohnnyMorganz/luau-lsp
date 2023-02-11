#pragma once
#include <optional>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

std::optional<std::string> getParentPath(const std::string& path);
std::optional<std::string> getAncestorPath(const std::string& path, const std::string& ancestorName);
std::string codeBlock(const std::string& language, const std::string& code);
std::optional<std::string> readFile(const std::filesystem::path& filePath);
void trim_start(std::string& str);
void trim_end(std::string& str);
void trim(std::string& str);
std::string& toLower(std::string& str);
bool endsWith(const std::string_view& str, const std::string_view& suffix);
bool replace(std::string& str, const std::string& from, const std::string& to);
void replaceAll(std::string& str, const std::string& from, const std::string& to);

template<class K, class V>
inline bool contains(const std::unordered_map<K, V>& map, const K& value)
{
    return map.find(value) != map.end();
}