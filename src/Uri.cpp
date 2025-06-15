// Based off https://github.com/microsoft/vscode-uri/blob/6dec22d7dcc6c63c30343d3a8d56050d0078cb6a/src/uri.ts
#include <optional>
#include <cctype>
#include "LSP/Uri.hpp"
#include "LSP/Utils.hpp"
#include "Luau/StringUtils.h"
#include "LuauFileUtils.hpp"

static std::string decodeURIComponent(const std::string& str)
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

static std::string percentDecode(const std::string& str)
{
    std::string out;
    for (size_t i = 0; i < str.length(); ++i)
    {
        if (str[i] == '%' && i + 2 < str.length() && isxdigit(str[i + 1]) && isxdigit(str[i + 2]))
        {
            out += decodeURIComponent(str.substr(i, 3));
            i += 2;
        }
        else
        {
            out += str[i];
        }
    }
    return out;
}

static std::optional<std::string> encode(char c)
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

// https://web.archive.org/web/20130128185825/http://es5.github.com:80/#x15.1.3.4
// https://stackoverflow.com/a/17708801
static std::string encodeURIComponent(const std::string& value)
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::value_type c : value)
    {
        // Keep alphanumeric and other accepted characters intact
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
        {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int((unsigned char)c);
        escaped << std::nouppercase;
    }

    return escaped.str();
}

static std::string encodeURIComponentFast(const std::string& uriComponent, bool isPath, bool isAuthority)
{
    std::optional<std::string> res = std::nullopt;
    size_t nativeEncodePos = std::string::npos;

    for (size_t pos = 0; pos < uriComponent.length(); pos++)
    {
        auto code = uriComponent.at(pos);

        // unreserved characters: https://tools.ietf.org/html/rfc3986#section-2.3
        if ((code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z') || (code >= '0' && code <= '9') || code == '-' || code == '.' ||
            code == '_' || code == '~' || (isPath && code == '/') || (isAuthority && code == '[') || (isAuthority && code == ']') ||
            (isAuthority && code == ':'))
        {
            // check if we are delaying native encode
            if (nativeEncodePos != std::string::npos)
            {
                *res += encodeURIComponent(uriComponent.substr(nativeEncodePos, pos - nativeEncodePos));
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
                    *res += encodeURIComponent(uriComponent.substr(nativeEncodePos, pos - nativeEncodePos));
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
        *res += encodeURIComponent(uriComponent.substr(nativeEncodePos));
    }

    return res.has_value() ? res.value() : uriComponent;
}

static std::string encodeURIComponentMinimal(const std::string& path, bool, bool)
{
    std::optional<std::string> res;
    for (size_t pos = 0; pos < path.length(); pos++)
    {
        auto ch = path[pos];
        if (ch == '#' || ch == '?')
        {
            if (!res.has_value())
            {
                res = path.substr(0, pos);
            }
            *res += *encode(ch);
        }
        else
        {
            if (res.has_value())
            {
                *res += path[pos];
            }
        }
    }
    return res.has_value() ? *res : path;
}

static Uri kNullUri = {"", "", "", "", ""};

Uri Uri::parse(const std::string& value)
{
    std::string scheme;
    std::string authority;
    std::string path;
    std::string query;
    std::string fragment;

    size_t current_pos = 0;
    size_t scheme_end = value.find(':');
    if (scheme_end != std::string::npos)
    {
        scheme = value.substr(0, scheme_end);
        current_pos = scheme_end + 1;
    }
    else
    {
        return kNullUri;
    }

    if (current_pos < value.length() && value.substr(current_pos, 2) == "//")
    {
        current_pos += 2;
        size_t authority_end = value.find_first_of("/?#", current_pos);
        if (authority_end != std::string::npos)
        {
            authority = value.substr(current_pos, authority_end - current_pos);
            current_pos = authority_end;
        }
        else
        {
            authority = value.substr(current_pos);
            current_pos = value.length();
        }
    }

    size_t path_end = value.find_first_of("?#", current_pos);
    if (path_end != std::string::npos)
    {
        path = value.substr(current_pos, path_end - current_pos);
        current_pos = path_end;
    }
    else
    {
        path = value.substr(current_pos);
        current_pos = value.length();
    }

    if (current_pos < value.length() && value[current_pos] == '?')
    {
        current_pos++;
        size_t fragment_start = value.find('#', current_pos);
        if (fragment_start != std::string::npos)
        {
            query = value.substr(current_pos, fragment_start - current_pos);
            current_pos = fragment_start;
        }
        else
        {
            query = value.substr(current_pos);
            current_pos = value.length();
        }
    }

    if (current_pos < value.length() && value[current_pos] == '#')
        fragment = value.substr(current_pos + 1);

    return {scheme, percentDecode(authority), percentDecode(path), percentDecode(query), percentDecode(fragment)};
}

Uri Uri::file(std::string_view fsPath)
{
    std::string authority = "";
    std::string path = std::string(fsPath);

// normalize to fwd-slashes on windows,
// on other systems bwd-slashes are valid
// filename character, eg /f\oo/ba\r.txt
#ifdef _WIN32
    std::replace(path.begin(), path.end(), '\\', '/');

    // For legacy reasons, VSCode uses a lower-case driver letter for Windows paths
    // We make it lower case here to normalise for all cases
    if (path.length() >= 2 && path[1] == ':' && isupper(path[0]))
    {
        path = std::string(1, tolower(path[0])) + path.substr(1);
    }
#endif

    // check for authority as used in UNC shares
    // or use the path as given
    if (path.length() >= 2 && path[0] == '/' && path[1] == '/')
    {
        auto idx = path.find('/', 2);
        if (idx == std::string::npos)
        {
            authority = path.substr(2);
            path = '/';
        }
        else
        {
            authority = path.substr(2, idx - 2);
            auto s = path.substr(idx);
            path = !s.empty() ? s : "/";
        }
    }

    return Uri("file", authority, path, "", "");
}

std::string Uri::fsPath() const
{
    if (!authority.empty() && path.length() > 1 && scheme == "file")
    {
        return "//" + authority + path;
    }
    else if (path.length() >= 3 && path.at(0) == '/' && isalpha(path.at(1)) && path.at(2) == ':')
    {
        return path.substr(1);
    }
    else
    {
        return path;
    }
}

// Encodes the Uri into a string representation
std::string Uri::toStringUncached(bool skipEncoding) const
{
    auto encoder = !skipEncoding ? encodeURIComponentFast : encodeURIComponentMinimal;
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
            idx = userinfo.rfind(':');
            if (idx == std::string::npos)
            {
                res += encoder(userinfo, false, false);
            }
            else
            {
                // <user>:<pass>@<auth>
                res += encoder(userinfo.substr(0, idx), false, false);
                res += ':';
                res += encoder(userinfo.substr(idx + 1), false, true);
            }
            res += '@';
        }
        mutAuthority = toLower(mutAuthority);
        toLower(mutAuthority);
        idx = mutAuthority.rfind(':');
        if (idx == std::string::npos)
        {
            res += encoder(mutAuthority, false, true);
        }
        else
        {
            // <auth>:<port>
            res += encoder(mutAuthority.substr(0, idx), false, true);
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
        res += encoder(mutPath, true, false);
    }
    if (!query.empty())
    {
        res += '?';
        res += encoder(query, false, false);
    }
    if (!fragment.empty())
    {
        res += '#';
        res += !skipEncoding ? encodeURIComponentFast(fragment, false, false) : fragment;
    }
    return res;
}

std::string Uri::toString(bool skipEncoding) const
{
    if (skipEncoding)
    {
        return toStringUncached(skipEncoding);
    }
    else
    {
        if (!cachedToString.empty())
            return cachedToString;
        return cachedToString = toStringUncached(skipEncoding);
    }
}

std::optional<Uri> Uri::parent() const
{
    auto parentParent = getParentPath(path);
    if (!parentParent)
        return std::nullopt;
    return Uri(scheme, authority, *parentParent, query, fragment);
}

std::string Uri::filename() const
{
    auto components = Luau::split(path, '/');
    if (components.empty())
        return "";

    return std::string(components.back());
}

std::string Uri::extension() const
{
    auto components = Luau::split(path, '/');
    if (components.empty())
        return "";

    auto parts = Luau::split(components.back(), '.');
    if (parts.size() == 1)
        return "";

    return "." + std::string(parts.back());
}

bool Uri::isFile() const
{
    if (scheme != "file")
        return false;

    return Luau::FileUtils::isFile(fsPath());
}

bool Uri::isDirectory() const
{
    if (scheme != "file")
        return false;

    return Luau::FileUtils::isDirectory(fsPath());
}

bool Uri::exists() const
{
    if (scheme != "file")
        return false;

    return Luau::FileUtils::exists(fsPath());
}

std::string Uri::lexicallyRelative(const Uri& base) const
{
    if (base.scheme != scheme || base.authority != authority)
    {
        // Different scheme or authority, target is already relative
        return path;
    }

    // Split paths into components
    std::vector<std::string_view> basePathComponents = Luau::split(base.path, '/');
    std::vector<std::string_view> targetPathComponents = Luau::split(path, '/');

    size_t min_size = std::min(basePathComponents.size(), targetPathComponents.size());
    size_t common_components = 0;

    // Find the common path components
    for (size_t i = 0; i < min_size; ++i)
    {
#if defined(_WIN32) || defined(__APPLE__)
        auto equal = toLower(std::string(basePathComponents[i])) == toLower(std::string(targetPathComponents[i]));
#else
        auto equal = basePathComponents[i] == targetPathComponents[i];
#endif
        if (equal)
            common_components++;
        else
            break;
    }

    // Handle if they are the same path
    if (common_components == basePathComponents.size() && common_components == targetPathComponents.size())
        return ".";

    std::string relative_path;
    for (size_t i = common_components; i < basePathComponents.size(); ++i)
    {
        relative_path += "..";
        if (i < basePathComponents.size() - 1 || common_components < targetPathComponents.size())
            relative_path += "/";
    }
    for (size_t i = common_components; i < targetPathComponents.size(); ++i)
    {
        relative_path += targetPathComponents[i];
        if (i < targetPathComponents.size() - 1)
            relative_path += "/";
    }

    return relative_path;
}

Uri Uri::resolvePath(std::string_view otherPath) const
{
    auto resolvedPath = this->path;
    bool slashAdded = false;

    if (Luau::FileUtils::isAbsolutePath(otherPath))
        resolvedPath = otherPath;
    else
    {
        if ((resolvedPath.empty() && authority.empty()) || (!resolvedPath.empty() && resolvedPath.back() == '/'))
            resolvedPath += otherPath;
        else
        {
            resolvedPath += '/';
            resolvedPath += otherPath;
        }
    }

    // We assume that the base is at least absolute, otherwise normalizePath will compute a relative path
    // If it's not, then modify it to prevent relative paths
    if (!Luau::FileUtils::isAbsolutePath(resolvedPath))
    {
        resolvedPath = "/" + resolvedPath;
        slashAdded = true;
    }

    resolvedPath = Luau::FileUtils::normalizePath(resolvedPath);

    if (slashAdded)
        resolvedPath = resolvedPath.substr(1);

    return {scheme, authority, resolvedPath, query, fragment};
}

bool Uri::isAncestorOf(const Uri& other) const
{
    if (scheme != other.scheme || authority != other.authority)
        return false;

#if defined(_WIN32) || defined(__APPLE__)
    return Luau::startsWith(toLower(other.path), toLower(path));
#else
    return Luau::startsWith(other.path, path);
#endif
}

size_t UriHash::operator()(const Uri& uri) const
{
    size_t hashValue = std::hash<std::string>()(uri.scheme);
    hashValue ^= std::hash<std::string>()(uri.authority) + 0x9e3779b9 + (hashValue << 6) + (hashValue >> 2);
#if defined(_WIN32) || defined(__APPLE__)
    hashValue ^= std::hash<std::string>()(toLower(uri.path)) + 0x9e3779b9 + (hashValue << 6) + (hashValue >> 2);
#else
    hashValue ^= std::hash<std::string>()(uri.path) + 0x9e3779b9 + (hashValue << 6) + (hashValue >> 2);
#endif
    hashValue ^= std::hash<std::string>()(uri.query) + 0x9e3779b9 + (hashValue << 6) + (hashValue >> 2);
    hashValue ^= std::hash<std::string>()(uri.fragment) + 0x9e3779b9 + (hashValue << 6) + (hashValue >> 2);
    return hashValue;
}

void from_json(const json& j, Uri& u)
{
    u = Uri::parse(j.get<std::string>());
}

void to_json(json& j, const Uri& u)
{
    j = u.toString();
}

bool isInitLuauFile(const Uri& uri)
{
    return uri.filename() == "init" || Luau::startsWith(uri.filename(), "init.");
}
