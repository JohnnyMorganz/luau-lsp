#pragma once
#include <optional>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <algorithm>

struct SourceNode;

std::optional<std::string> getParentPath(const std::string& path);
std::optional<std::string> getAncestorPath(const std::string& path, const std::string& ancestorName, const SourceNode* rootSourceNode);
std::string convertToScriptPath(std::string path);
std::string codeBlock(const std::string& language, const std::string& code);
std::optional<std::string> getHomeDirectory();
std::string resolvePath(const std::string& path);
bool isDataModel(const std::string& path);
void trim_start(std::string& str);
void trim_end(std::string& str);
void trim(std::string& str);
std::string removePrefix(const std::string& str, const std::string& prefix);
std::string toLower(std::string str);
std::string_view getFirstLine(const std::string_view& str);
bool endsWith(const std::string_view& str, const std::string_view& suffix);
std::string removeSuffix(const std::string& str, const std::string_view& suffix);
bool replace(std::string& str, const std::string& from, const std::string& to);
void replaceAll(std::string& str, const std::string& from, const std::string& to);

template<typename V>
inline bool contains(const std::vector<V>& vec, const V& value)
{
    return std::find(std::begin(vec), std::end(vec), value) != std::end(vec);
}

template<class K, class V>
inline bool contains(const std::unordered_map<K, V>& map, const K& value)
{
    return map.find(value) != map.end();
}

template<class K, class V>
inline bool contains(const std::map<K, V>& map, const K& value)
{
    return map.find(value) != map.end();
}
