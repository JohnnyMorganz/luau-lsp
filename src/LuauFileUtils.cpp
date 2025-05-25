#include "LuauFileUtils.hpp"

namespace Luau::FileUtils {
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
}