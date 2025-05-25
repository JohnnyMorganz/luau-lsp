// Based off https://github.com/microsoft/vscode-uri/blob/main/src/uri.ts
#pragma once
#include <filesystem>
#include <regex>
#include "nlohmann/json.hpp"
#include "LSP/Utils.hpp"

using json = nlohmann::json;

// implements a bit of https://tools.ietf.org/html/rfc3986#section-5
static std::string _referenceResolution(const std::string& scheme, const std::string& path)
{
    std::string res = path;

    // the slash-character is our 'default base' as we don't
    // support constructing URIs relative to other URIs. This
    // also means that we alter and potentially break paths.
    // see https://tools.ietf.org/html/rfc3986#section-5.1.4
    if (scheme == "https" || scheme == "http" || scheme == "file")
    {
        if (res.empty())
        {
            res = "/";
        }
        else if (res[0] != '/')
        {
            res = '/' + path;
        }
    }

    return res;
}

class Uri
{
public:
    std::string scheme;
    std::string authority;
    std::string path;
    std::string query;
    std::string fragment;

public:
    Uri() = default;

    Uri(const std::string& scheme, std::string authority, const std::string& path, std::string query = "", std::string fragment = "")
        : scheme(std::move(scheme))
        , authority(std::move(authority))
        , path(_referenceResolution(scheme, path))
        , query(std::move(query))
        , fragment(std::move(fragment))
    {
        // TODO: validate?
    }

    bool operator==(const Uri& other) const
    {
#if defined(_WIN32) || defined(__APPLE__)
        return scheme == other.scheme && authority == other.authority && toLower(path) == toLower(other.path) && query == other.query &&
               fragment == other.fragment;
#else
        return scheme == other.scheme && authority == other.authority && path == other.path && query == other.query && fragment == other.fragment;
#endif
    }

    bool operator!=(const Uri& other) const
    {
        return !operator==(other);
    }

    static Uri parse(const std::string& value);
    static Uri file(const std::filesystem::path& fsPath);

    // TODO: make this conversion explicit
    operator std::filesystem::path()
    {
        return fsPath();
    }

    std::filesystem::path fsPath() const;

private:
    mutable std::string cachedToString;
    std::string toStringUncached(bool skipEncoding = false) const;

public:
    // Encodes the Uri into a string representation
    std::string toString(bool skipEncoding = false) const;

    // Returns the parent path of this URI, if it exists
    std::optional<Uri> parent() const;

    /// Returns the filename of this URI, if it exists
    /// Equivalent to the last component of the Uri.path;
    std::string filename() const;

    /// Returns the extension of a path, if it exists
    /// If the path has no components, or no extension, then returns an empty string
    std::string extension() const;

    /// Checks whether this URI corresponds to a directory. Performs a file-system call.
    /// Always returns false if scheme is not 'file'
    bool isDirectory() const;

    /// Checks whether this URI corresponds to a path that exists on the file system. Performs a file-system call.
    /// Always returns false if scheme is not 'file'
    bool exists() const;

    // Returns a string path that is lexically relative to the other URI, similar to std::filesystem::path.lexically_relative()
    std::string lexicallyRelative(const Uri& base) const;

    // Resolves a path against the path of the URI
    // '/' is used as the directory separation character.
    //
    // The resolved path will be normalized. That means:
    //  - all '..' and '.' segments are resolved.
    //  - multiple, sequential occurences of '/' are replaced by a single instance of '/'.
    //  - trailing separators are removed.
    //
    // If 'otherPath' is an absolute path, then the current path of the Uri is replaced
    Uri resolvePath(std::string_view otherPath) const;

    // Returns whether the current Uri is an ancestor of the other URI
    bool isAncestorOf(const Uri& other) const;
};

struct UriHash
{
    size_t operator()(const Uri& uri) const;
};

void from_json(const json& j, Uri& u);
void to_json(json& j, const Uri& u);

namespace nlohmann
{
template<typename T>
struct adl_serializer<std::unordered_map<Uri, T, UriHash>>
{
    static void to_json(json& j, const std::unordered_map<Uri, T, UriHash>& opt)
    {
        std::unordered_map<std::string, T> stringifiedKeys;
        for (const auto& [k, v] : opt)
            stringifiedKeys[k.toString()] = v;
        j = stringifiedKeys;
    }

    static void from_json(const json& j, std::unordered_map<Uri, T, UriHash>& opt)
    {
        std::unordered_map<std::string, T> result = j;
        for (const auto& [k, v] : result)
            opt[Uri::parse(k)] = v;
    }
};
} // namespace nlohmann
