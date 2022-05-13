// Based off https://github.com/microsoft/vscode-uri/blob/main/src/uri.ts
#pragma once
#include <filesystem>
#include <regex>
#include <optional>
#include <ctype.h>
#include "nlohmann/json.hpp"
using json = nlohmann::json;

class Uri;

std::string percentDecode(const std::string& str)
{
    std::string res;
    res.reserve(str.length());

    for (size_t i = 0; i < str.length(); i++)
    {
        auto ch = str[i];
        if (ch == '%' && (i + 2) < str.length())
        {
            auto hex = str.substr(i + 1, 2);
            res += static_cast<char>(std::stol(hex.c_str(), nullptr, 16));
            i += 2;
        }
        else if (ch == '+')
        {
            res += ' ';
        }
        else
        {
            res += ch;
        }
    }
    return res;
}

std::optional<std::string> encode(char c)
{
    switch (c)
    {
    // gen-delims
    case ':':
        return "%3A";
    case '/':
        return "%2F";
    case '?':
        return "%3F";
    case '#':
        return "%23";
    case '[':
        return "%5B";
    case ']':
        return "%5D";
    case '@':
        return "%40";

    // sub-delims
    case '!':
        return "%21";
    case '$':
        return "%24";
    case '&':
        return "%26";
    case '\'':
        return "%27";
    case '(':
        return "%28";
    case ')':
        return "%29";
    case '*':
        return "%2A";
    case '+':
        return "%2B";
    case ',':
        return "%2C";
    case ';':
        return "%3B";
    case '=':
        return "%3D";

    case ' ':
        return "%20";

    default:
        return std::nullopt;
    }
}

std::string encodeURIComponent(std::string uriComponent, bool allowSlash = false)
{
    std::optional<std::string> res = std::nullopt;
    size_t nativeEncodePos = std::string::npos;

    for (size_t pos = 0; pos < uriComponent.length(); pos++)
    {
        auto code = uriComponent.at(pos);

        // unreserved characters: https://tools.ietf.org/html/rfc3986#section-2.3
        if (isalpha(code) || isdigit(code) || code == '-' || code == '.' || code == '_' || code == '~' || (allowSlash && code == '/'))
        {
            // check if we are delaying native encode
            if (nativeEncodePos != std::string::npos)
            {
                *res += encodeURIComponent(uriComponent.substr(nativeEncodePos, pos));
                nativeEncodePos = std::string::npos;
            }
            // check if we write into a new string (by default we try to return the param)
            if (res.has_value())
            {
                *res += uriComponent.at(pos);
            }
        }
        else
        {
            // encoding needed, we need to allocate a new string
            if (!res.has_value())
            {
                res = uriComponent.substr(0, pos);
            }

            // check with default table first
            auto escaped = encode(code);
            if (escaped.has_value())
            {

                // check if we are delaying native encode
                if (nativeEncodePos != std::string::npos)
                {
                    *res += encodeURIComponent(uriComponent.substr(nativeEncodePos, pos));
                    nativeEncodePos = std::string::npos;
                }

                // append escaped variant to result
                *res += *escaped;
            }
            else if (nativeEncodePos == std::string::npos)
            {
                // use native encode only when needed
                nativeEncodePos = pos;
            }
        }
    }

    if (nativeEncodePos != std::string::npos)
    {
        *res += encodeURIComponent(uriComponent.substr(nativeEncodePos), allowSlash);
    }

    return res.has_value() ? res.value() : uriComponent;
}

// implements a bit of https://tools.ietf.org/html/rfc3986#section-5
std::string _referenceResolution(const std::string& scheme, const std::string& path)
{
    auto res = path;

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

    Uri() {}

    Uri(const std::string& scheme, const std::string& authority, const std::string& path, const std::string& query, const std::string& fragment)
        : scheme(scheme)
        , authority(authority)
        , path(_referenceResolution(scheme, path))
        , query(query)
        , fragment(fragment)
    {
        // TODO: validate?
    }

    static Uri parse(const std::string& value)
    {
        const std::regex REGEX_EXPR("^(([^:\\/?#]+?):)?(\\/\\/([^\\/?#]*))?([^?#]*)(\\?([^#]*))?(#(.*))?");
        std::smatch match;
        if (std::regex_match(value, match, REGEX_EXPR))
        {
            // match[0] is whole string
            return Uri(match[2], percentDecode(match[4]), percentDecode(match[5]), percentDecode(match[7]), percentDecode(match[9]));
        }
        else
        {
            // TODO: should we error?
            return Uri("", "", "", "", "");
        }
    }

    static Uri file(const std::filesystem::path& fsPath)
    {
        return Uri("file", "", fsPath.generic_string(), "", "");
    }

    operator std::filesystem::path()
    {
        return fsPath();
    }

    std::filesystem::path fsPath() const
    {
        if (path.length() >= 3 && path.at(0) == '/' && isalpha(path.at(1)) && path.at(2) == ':')
        {
            return path.substr(1);
        }
        else
        {
            return path;
        }
    }

    // Encodes the Uri into a string representation
    std::string toString() const
    {
        std::string res;
        if (!scheme.empty())
        {
            res += scheme;
            res += ':';
        }
        if (!authority.empty() || scheme == "file")
        {
            res += '/';
            res += '/';
        }
        if (!authority.empty())
        {
            auto mutAuthority = authority;
            auto idx = mutAuthority.find('@');
            if (idx != std::string::npos)
            {
                // <user>@<auth>
                auto userinfo = mutAuthority.substr(0, idx);
                mutAuthority = mutAuthority.substr(idx + 1);
                idx = userinfo.find(':');
                if (idx == std::string::npos)
                {
                    res += encodeURIComponent(userinfo, false);
                }
                else
                {
                    // <user>:<pass>@<auth>
                    res += encodeURIComponent(userinfo.substr(0, idx), false);
                    res += ':';
                    res += encodeURIComponent(userinfo.substr(idx + 1), false);
                }
                res += '@';
            }
            std::for_each(mutAuthority.begin(), mutAuthority.end(),
                [](unsigned char c)
                {
                    return std::tolower(c);
                });
            idx = mutAuthority.find(':');
            if (idx == std::string::npos)
            {
                res += encodeURIComponent(mutAuthority, false);
            }
            else
            {
                // <auth>:<port>
                res += encodeURIComponent(mutAuthority.substr(0, idx), false);
                res += mutAuthority.substr(idx);
            }
        }
        if (!path.empty())
        {
            // lower-case windows drive letters in /C:/fff or C:/fff
            auto mutPath = path;
            if (mutPath.length() >= 3 && mutPath.at(0) == '/' && mutPath.at(2) == ':')
            {
                auto code = path.at(1);
                if (isupper(code))
                {
                    mutPath = "/" + std::string(1, tolower(code)) + ":" + mutPath.substr(3); // "/c:".length === 3
                }
            }
            else if (mutPath.length() >= 2 && mutPath.at(1) == ':')
            {
                auto code = path.at(0);
                if (isupper(code))
                {
                    mutPath = std::string(1, tolower(code)) + ":" + mutPath.substr(2); // "/c:".length === 2
                }
            }
            // encode the rest of the path
            res += encodeURIComponent(mutPath, true);
        }
        if (!query.empty())
        {
            res += '?';
            res += encodeURIComponent(query, false);
        }
        if (!fragment.empty())
        {
            res += '#';
            res += encodeURIComponent(fragment, false);
        }
        return res;
    }
};

void from_json(const json& j, Uri& u)
{
    u = Uri::parse(j.get<std::string>());
}

void to_json(json& j, const Uri& u)
{
    j = u.toString();
}