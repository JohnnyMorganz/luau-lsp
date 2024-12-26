#include "LSP/Utils.hpp"
#include "Luau/StringUtils.h"
#include "Platform/RobloxPlatform.hpp"
#include <algorithm>
#include <fstream>

std::optional<std::string> getParentPath(const std::string& path)
{
    if (path == "" || path == "." || path == "/")
        return std::nullopt;

    std::string::size_type slash = path.find_last_of("\\/", path.size() - 1);

    if (slash == 0)
        return "/";

    if (slash != std::string::npos)
        return path.substr(0, slash);

    return "";
}

/// Returns a path at the ancestor point.
/// i.e., for game/ReplicatedStorage/Module/Child/Foo, and ancestor == Module, returns game/ReplicatedStorage/Module
std::optional<std::string> getAncestorPath(const std::string& path, const std::string& ancestorName, const SourceNodePtr& rootSourceNode)
{
    // We want to remove the child from the path name in case the ancestor has the same name as the child
    auto parentPath = getParentPath(path);
    if (!parentPath)
        return std::nullopt;

    // Append a "/" to the end of the parentPath to make searching easier
    auto parentPathWithSlash = *parentPath + "/";

    auto ancestor = parentPathWithSlash.rfind(ancestorName + "/");
    if (ancestor != std::string::npos)
    {
        // We need to ensure that the character before the ancestor is a / (or the ancestor is the very beginning)
        // And also make sure that the character after the ancestor is a / (or the ancestor is at the very end)
        if (ancestor == 0 || parentPathWithSlash.at(ancestor - 1) == '/')
        {
            return parentPathWithSlash.substr(0, ancestor + ancestorName.size());
        }
    }

    // At this point we know there is definitely no ancestor with the same name within the path
    // We can return ProjectRoot if project is not a DataModel and rootSourceNode.name == ancestorName
    if (rootSourceNode && !isDataModel(parentPathWithSlash) && ancestorName == rootSourceNode->name)
    {
        return "ProjectRoot";
    }

    return std::nullopt;
}

std::string convertToScriptPath(const std::string& path)
{
    std::filesystem::path p(path);
    std::string output = "";
    for (auto it = p.begin(); it != p.end(); ++it)
    {
        auto str = it->string();
        if (str.find(' ') != std::string::npos)
            output += "[\"" + str + "\"]";
        else if (str == ".")
        {
            if (it == p.begin())
                output += "script";
        }
        else if (str == "..")
        {
            if (it == p.begin())
                output += "script.Parent";
            else
                output += ".Parent";
        }
        else
        {
            if (it != p.begin())
                output += ".";
            output += str;
        }
    }
    return output;
}


std::string codeBlock(const std::string& language, const std::string& code)
{
    return "```" + language + "\n" + code + "\n" + "```";
}

std::optional<std::string> readFile(const std::filesystem::path& filePath)
{
    std::ifstream fileContents;
    fileContents.open(filePath);

    std::string output;
    std::stringstream buffer;

    if (fileContents)
    {
        buffer << fileContents.rdbuf();
        output = buffer.str();
        return output;
    }
    else
    {
        return std::nullopt;
    }
}

std::optional<std::filesystem::path> getHomeDirectory()
{
    if (const char* home = getenv("HOME"))
    {
        return home;
    }
    else if (const char* userProfile = getenv("USERPROFILE"))
    {
        return userProfile;
    }
    else
    {
        return std::nullopt;
    }
}

// Resolves a filesystem path, including any tilde expansion
std::filesystem::path resolvePath(const std::filesystem::path& path)
{
    if (Luau::startsWith(path.generic_string(), "~/"))
    {
        if (auto home = getHomeDirectory())
            return home.value() / path.string().substr(2);
        else
            // TODO: should we error / return an optional here instead?
            return path;
    }
    else
        return path;
}

bool isDataModel(const std::string& path)
{
    return Luau::startsWith(path, "game/");
}


void trim_start(std::string& str)
{
    str.erase(0, str.find_first_not_of(" \n\r\t"));
}

std::string removePrefix(const std::string& str, const std::string& prefix)
{
    if (Luau::startsWith(str, prefix))
        return str.substr(prefix.length());
    return str;
}


void trim_end(std::string& str)
{
    str.erase(str.find_last_not_of(" \n\r\t") + 1);
}

void trim(std::string& str)
{
    trim_start(str);
    trim_end(str);
}

std::string& toLower(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(),
        [](unsigned char c)
        {
            return std::tolower(c);
        });
    return str;
}

std::string_view getFirstLine(const std::string_view& str)
{
    size_t eol_char = str.find('\n');
    if (eol_char == std::string::npos)
        return str;
    return str.substr(0, eol_char);
}

bool endsWith(const std::string_view& str, const std::string_view& suffix)
{
    return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

bool replace(std::string& str, const std::string& from, const std::string& to)
{
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

void replaceAll(std::string& str, const std::string& from, const std::string& to)
{
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

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