#pragma once

#include <string>
#include <string_view>
#include <vector>

// TODO: Use Luau FileUtils directly once it is namespaced
namespace Luau::FileUtils {
bool isAbsolutePath(std::string_view path);
std::string normalizePath(std::string_view path);
std::string resolvePath(std::string_view relativePath, std::string_view baseFilePath);
std::vector<std::string_view> splitPath(std::string_view path);
}
