#include "LuauFileUtils.hpp"

#include "Luau/Common.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#ifdef _WIN32
static std::wstring fromUtf8(const std::string& path)
{
    size_t result = MultiByteToWideChar(CP_UTF8, 0, path.data(), int(path.size()), nullptr, 0);
    LUAU_ASSERT(result);

    std::wstring buf(result, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.data(), int(path.size()), &buf[0], int(buf.size()));

    return buf;
}

static std::string toUtf8(const std::wstring& path)
{
    size_t result = WideCharToMultiByte(CP_UTF8, 0, path.data(), int(path.size()), nullptr, 0, nullptr, nullptr);
    LUAU_ASSERT(result);

    std::string buf(result, '\0');
    WideCharToMultiByte(CP_UTF8, 0, path.data(), int(path.size()), &buf[0], int(buf.size()), nullptr, nullptr);

    return buf;
}
#endif

namespace Luau::FileUtils
{
bool isAbsolutePath(std::string_view path)
{
#ifdef _WIN32
    // Must either begin with "X:/", "X:\", "/", or "\", where X is a drive letter
    return (path.size() >= 3 && isalpha(path[0]) && path[1] == ':' && (path[2] == '/' || path[2] == '\\')) ||
           (path.size() >= 1 && (path[0] == '/' || path[0] == '\\'));
#else
    // Must begin with '/'
    return path.size() >= 1 && path[0] == '/';
#endif
}

std::optional<std::string> readFile(const std::string& name)
{
#ifdef _WIN32
    FILE* file = _wfopen(fromUtf8(name).c_str(), L"rb");
#else
    FILE* file = fopen(name.c_str(), "rb");
#endif

    if (!file)
        return std::nullopt;

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    if (length < 0)
    {
        fclose(file);
        return std::nullopt;
    }
    fseek(file, 0, SEEK_SET);

    std::string result(length, 0);

    size_t read = fread(result.data(), 1, length, file);
    fclose(file);

    if (read != size_t(length))
        return std::nullopt;

    // LUAU-LSP DEVIATION: We don't remove shebang here, that is handled in TextDocument

    return result;
}

std::optional<std::string> getCurrentWorkingDirectory()
{
    // 2^17 - derived from the Windows path length limit
    constexpr size_t maxPathLength = 131072;
    constexpr size_t initialPathLength = 260;

    std::string directory(initialPathLength, '\0');
    char* cstr = nullptr;

    while (!cstr && directory.size() <= maxPathLength)
    {
#ifdef _WIN32
        cstr = _getcwd(directory.data(), static_cast<int>(directory.size()));
#else
        cstr = getcwd(directory.data(), directory.size());
#endif
        if (cstr)
        {
            directory.resize(strlen(cstr));
            return directory;
        }
        else if (errno != ERANGE || directory.size() * 2 > maxPathLength)
        {
            return std::nullopt;
        }
        else
        {
            directory.resize(directory.size() * 2);
        }
    }
    return std::nullopt;
}

template<typename Ch>
static void joinPaths(std::basic_string<Ch>& str, const Ch* lhs, const Ch* rhs)
{
    str = lhs;
    if (!str.empty() && str.back() != '/' && str.back() != '\\' && *rhs != '/' && *rhs != '\\')
        str += '/';
    str += rhs;
}

#ifdef _WIN32
static bool traverseDirectoryRec(const std::wstring& path, const std::function<void(const std::string& name)>& callback, bool recursive)
{
    std::wstring query = path + std::wstring(L"/*");

    WIN32_FIND_DATAW data;
    HANDLE h = FindFirstFileW(query.c_str(), &data);

    if (h == INVALID_HANDLE_VALUE)
        return false;

    std::wstring buf;

    do
    {
        if (wcscmp(data.cFileName, L".") != 0 && wcscmp(data.cFileName, L"..") != 0)
        {
            joinPaths(buf, path.c_str(), data.cFileName);

            if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            {
                // Skip reparse points to avoid handling cycles
            }
            else if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && recursive)
            {
                traverseDirectoryRec(buf, callback, recursive);
            }
            else
            {
                callback(toUtf8(buf));
            }
        }
    } while (FindNextFileW(h, &data));

    FindClose(h);

    return true;
}

bool traverseDirectory(const std::string& path, const std::function<void(const std::string& name)>& callback)
{
    return traverseDirectoryRec(fromUtf8(path), callback, /* recursive= */ false);
}

bool traverseDirectoryRecursive(const std::string& path, const std::function<void(const std::string& name)>& callback)
{
    return traverseDirectoryRec(fromUtf8(path), callback, /* recursive= */ true);
}
#else
static bool traverseDirectoryRec(const std::string& path, const std::function<void(const std::string& name)>& callback, bool recursive)
{
    int fd = open(path.c_str(), O_DIRECTORY);
    DIR* dir = fdopendir(fd);

    if (!dir)
        return false;

    std::string buf;

    while (dirent* entry = readdir(dir))
    {
        const dirent& data = *entry;

        if (strcmp(data.d_name, ".") != 0 && strcmp(data.d_name, "..") != 0)
        {
            joinPaths(buf, path.c_str(), data.d_name);

#if defined(DTTOIF)
            mode_t mode = DTTOIF(data.d_type);
#else
            mode_t mode = 0;
#endif

            // we need to stat an UNKNOWN to be able to tell the type
            if ((mode & S_IFMT) == 0)
            {
                struct stat st = {};
#ifdef _ATFILE_SOURCE
                fstatat(fd, data.d_name, &st, 0);
#else
                lstat(buf.c_str(), &st);
#endif

                mode = st.st_mode;
            }

            if (mode == S_IFDIR && recursive)
            {
                traverseDirectoryRec(buf, callback, recursive);
            }
            else if (mode == S_IFREG)
            {
                callback(buf);
            }
            else if (mode == S_IFLNK)
            {
                // Skip symbolic links to avoid handling cycles
            }
        }
    }

    closedir(dir);

    return true;
}

bool traverseDirectory(const std::string& path, const std::function<void(const std::string& name)>& callback)
{
    return traverseDirectoryRec(path, callback, /* recursive= */ false);
}

bool traverseDirectoryRecursive(const std::string& path, const std::function<void(const std::string& name)>& callback)
{
    return traverseDirectoryRec(path, callback, /* recursive= */ true);
}
#endif

bool exists(const std::string& path)
{
#ifdef _WIN32
    DWORD fileAttributes = GetFileAttributesW(fromUtf8(path).c_str());
    if (fileAttributes == INVALID_FILE_ATTRIBUTES)
        return false;
    return true;
#else
    struct stat st = {};
    return lstat(path.c_str(), &st) == 0;
#endif
}

bool isFile(const std::string& path)
{
#ifdef _WIN32
    DWORD fileAttributes = GetFileAttributesW(fromUtf8(path).c_str());
    if (fileAttributes == INVALID_FILE_ATTRIBUTES)
        return false;
    return (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    struct stat st = {};
    lstat(path.c_str(), &st);
    return (st.st_mode & S_IFMT) == S_IFREG;
#endif
}

bool isDirectory(const std::string& path)
{
#ifdef _WIN32
    DWORD fileAttributes = GetFileAttributesW(fromUtf8(path).c_str());
    if (fileAttributes == INVALID_FILE_ATTRIBUTES)
        return false;
    return (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st = {};
    lstat(path.c_str(), &st);
    return (st.st_mode & S_IFMT) == S_IFDIR;
#endif
}

// Returns the normal/canonical form of a path (e.g. "../subfolder/../module.luau" -> "../module.luau")
std::string normalizePath(std::string_view path)
{
    return resolvePath(path, "");
}

// Takes a path that is relative to the file at baseFilePath and returns the path explicitly rebased onto baseFilePath.
// For absolute paths, baseFilePath will be ignored, and this function will resolve the path to a canonical path:
// (e.g. "/Users/.././Users/johndoe" -> "/Users/johndoe").
std::string resolvePath(std::string_view path, std::string_view baseFilePath)
{
    std::vector<std::string_view> pathComponents;
    std::vector<std::string_view> baseFilePathComponents;

    // Dependent on whether the final resolved path is absolute or relative
    // - if relative (when path and baseFilePath are both relative), resolvedPathPrefix remains empty
    // - if absolute (if either path or baseFilePath are absolute), resolvedPathPrefix is "C:\", "/", etc.
    std::string resolvedPathPrefix;
    bool isResolvedPathRelative = false;

    if (isAbsolutePath(path))
    {
        // path is absolute, we use path's prefix and ignore baseFilePath
        size_t afterPrefix = path.find_first_of("\\/") + 1;
        resolvedPathPrefix = path.substr(0, afterPrefix);
        pathComponents = splitPath(path.substr(afterPrefix));
    }
    else
    {
        size_t afterPrefix = baseFilePath.find_first_of("\\/") + 1;
        baseFilePathComponents = splitPath(baseFilePath.substr(afterPrefix));
        if (isAbsolutePath(baseFilePath))
        {
            // path is relative and baseFilePath is absolute, we use baseFilePath's prefix
            resolvedPathPrefix = baseFilePath.substr(0, afterPrefix);
        }
        else
        {
            // path and baseFilePath are both relative, we do not set a prefix (resolved path will be relative)
            isResolvedPathRelative = true;
        }
        pathComponents = splitPath(path);
    }

    // Remove filename from components
    if (!baseFilePathComponents.empty())
        baseFilePathComponents.pop_back();

    // Resolve the path by applying pathComponents to baseFilePathComponents
    int numPrependedParents = 0;
    for (std::string_view component : pathComponents)
    {
        if (component == "..")
        {
            if (baseFilePathComponents.empty())
            {
                if (isResolvedPathRelative)
                    numPrependedParents++; // "../" will later be added to the beginning of the resolved path
            }
            else if (baseFilePathComponents.back() != "..")
            {
                baseFilePathComponents.pop_back(); // Resolve cases like "folder/subfolder/../../file" to "file"
            }
        }
        else if (component != "." && !component.empty())
        {
            baseFilePathComponents.push_back(component);
        }
    }

    // Create resolved path prefix for relative paths
    if (isResolvedPathRelative)
    {
        if (numPrependedParents > 0)
        {
            resolvedPathPrefix.reserve(numPrependedParents * 3);
            for (int i = 0; i < numPrependedParents; i++)
            {
                resolvedPathPrefix += "../";
            }
        }
        else
        {
            resolvedPathPrefix = "./";
        }
    }

    // Join baseFilePathComponents to form the resolved path
    std::string resolvedPath = resolvedPathPrefix;
    for (auto iter = baseFilePathComponents.begin(); iter != baseFilePathComponents.end(); ++iter)
    {
        if (iter != baseFilePathComponents.begin())
            resolvedPath += "/";

        resolvedPath += *iter;
    }
    if (resolvedPath.size() > resolvedPathPrefix.size() && resolvedPath.back() == '/')
    {
        // Remove trailing '/' if present
        resolvedPath.pop_back();
    }
    return resolvedPath;
}

std::vector<std::string_view> splitPath(std::string_view path)
{
    std::vector<std::string_view> components;

    size_t pos = 0;
    size_t nextPos = path.find_first_of("\\/", pos);

    while (nextPos != std::string::npos)
    {
        components.push_back(path.substr(pos, nextPos - pos));
        pos = nextPos + 1;
        nextPos = path.find_first_of("\\/", pos);
    }
    components.push_back(path.substr(pos));

    return components;
}

std::string joinPaths(std::string_view lhs, std::string_view rhs)
{
    std::string result = std::string(lhs);
    if (!result.empty() && result.back() != '/' && result.back() != '\\')
        result += '/';
    result += rhs;
    return result;
}
} // namespace Luau::FileUtils
