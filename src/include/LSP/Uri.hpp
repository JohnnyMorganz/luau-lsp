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
    // Returns a string path that is lexically relative to the other URI, similar to std::filesystem::path.lexically_relative()
    std::string lexicallyRelative(const Uri& base) const;
};

struct UriHash
{
    size_t operator()(const Uri& uri) const;
};

void from_json(const json& j, Uri& u);
void to_json(json& j, const Uri& u);
