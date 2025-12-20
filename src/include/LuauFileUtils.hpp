#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <functional>
#include <vector>

// TODO: Use Luau FileUtils directly once it is namespaced
namespace Luau::FileUtils
{
#ifdef _WIN32
std::wstring fromUtf8(const std::string& path);
#endif

bool isAbsolutePath(std::string_view path);

std::optional<std::string> readFile(const std::string& name);

std::optional<std::string> getCurrentWorkingDirectory();

bool exists(const std::string& path);
bool isFile(const std::string& path);
bool isDirectory(const std::string& path);
// LUAU-LSP DEVIATION: 'traverseDirectory' is non-recursive and matches on directory names. There is a separate recursive function
bool traverseDirectory(const std::string& path, const std::function<void(const std::string& name)>& callback);
bool traverseDirectoryRecursive(const std::string& path, const std::function<void(const std::string& name)>& callback);

std::string normalizePath(std::string_view path);
std::string resolvePath(std::string_view relativePath, std::string_view baseFilePath);
std::vector<std::string_view> splitPath(std::string_view path);
std::string joinPaths(std::string_view lhs, std::string_view rhs);
} // namespace Luau::FileUtils
