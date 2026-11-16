#define RYML_SINGLE_HDR_DEFINE_NOW
#include "LSP/JsonTomlSyntaxParser.hpp"
#include "Luau/StringUtils.h"

#include <cctype>
#include <optional>

std::string jsoncToJson(std::string_view source)
{
    std::string out;
    out.reserve(source.size());

    // A comma is only trailing if the next significant character closes the container, so remember its position until we know
    std::optional<size_t> pendingComma;
    char previous = '\0'; // The last significant (non-whitespace, non-comment) character written

    for (size_t i = 0; i < source.size(); ++i)
    {
        const char c = source[i];

        if (c == '"')
        {
            const size_t start = i++;
            while (i < source.size() && source[i] != '"')
                i += source[i] == '\\' ? 2 : 1;
            out.append(source.substr(start, i - start + 1));
            pendingComma.reset();
            previous = '"';
        }
        else if (c == '/' && i + 1 < source.size() && source[i + 1] == '/')
        {
            while (i + 1 < source.size() && source[i + 1] != '\n')
                ++i;
        }
        else if (c == '/' && i + 1 < source.size() && source[i + 1] == '*')
        {
            const size_t end = source.find("*/", i + 2);
            if (end == std::string_view::npos)
            {
                // Unterminated comment: leave it in place so that the JSON parser reports an error
                out.append(source.substr(i));
                break;
            }
            i = end + 1;
            out += ' ';
        }
        else if (c == ',')
        {
            out += c;
            // A comma that does not follow a value (e.g. `[,]` or `[1,,]`) is never valid, even if a closing bracket follows
            const bool followsValue = previous != '\0' && previous != '[' && previous != '{' && previous != ',';
            pendingComma = followsValue ? std::optional<size_t>(out.size() - 1) : std::nullopt;
            previous = ',';
        }
        else if (c == '}' || c == ']')
        {
            if (pendingComma)
                out[*pendingComma] = ' ';
            pendingComma.reset();
            out += c;
            previous = c;
        }
        else
        {
            out += c;
            if (!std::isspace(static_cast<unsigned char>(c)))
            {
                pendingComma.reset();
                previous = c;
            }
        }
    }

    return out;
}

std::string jsonValueToLuau(const nlohmann::json& val)
{
    if (val.is_string())
    {
        return '"' + Luau::escape(val.get<std::string>()) + '"';
    }
    else if (val.is_number() || val.is_boolean())
    {
        return val.dump();
    }
    else if (val.is_null())
    {
        return "nil";
    }
    else if (val.is_array())
    {
        std::string out = "{";
        for (auto& elem : val)
        {
            out += jsonValueToLuau(elem);
            out += ";";
        }

        out += "}";
        return out;
    }
    else if (val.is_object())
    {
        std::string out = "{";

        for (auto& [key, value] : val.items())
        {
            out += "[\"" + Luau::escape(key) + "\"] = ";
            out += jsonValueToLuau(value);
            out += ";";
        }

        out += "}";
        return out;
    }
    else
    {
        return ""; // TODO: should we error here?
    }
}

std::string tomlValueToLuau(const toml::value& val)
{
    if (val.is_string())
    {
        std::string str = val.as_string();
        return '"' + Luau::escape(str) + '"';
    }
    else if (val.is_integer() || val.is_floating() || val.is_boolean())
    {
        return toml::format(val);
    }
    else if (val.is_uninitialized())
    {
        // unreachable
        return "nil";
    }
    else if (val.is_array())
    {
        std::string out = "{";
        for (auto& elem : val.as_array())
        {
            out += tomlValueToLuau(elem);
            out += ";";
        }

        out += "}";
        return out;
    }
    else if (val.is_table())
    {
        std::string out = "{";
        for (auto& [key, value] : val.as_table())
        {
            out += "[\"" + Luau::escape(key) + "\"] = ";
            out += tomlValueToLuau(value);
            out += ";";
        }

        out += "}";
        return out;
    }
    // TODO: support datetime?
    else
    {
        return ""; // TODO: should we error here?
    }
}

std::string yamlValueToLuau(ryml::ConstNodeRef node)
{
    if (node.is_seq())
    {
        std::string out = "{";
        for (const auto& child : node.children())
        {
            out += yamlValueToLuau(child);
            out += ";";
        }
        out += "}";
        return out;
    }

    if (node.is_map())
    {
        std::string out = "{";
        for (const auto& child : node.children())
        {
            ryml::csubstr keyView = child.key();
            std::string key(keyView.str, keyView.len);
            out += "[\"" + Luau::escape(key) + "\"] = ";
            out += yamlValueToLuau(child);
            out += ";";
        }
        out += "}";
        return out;
    }

    if (!node.has_val())
    {
        return "nil";
    }

    ryml::csubstr val = node.val();

    if (val.empty() || val == "~" || val == "null" || val == "Null" || val == "NULL")
    {
        return "nil";
    }

    std::string strVal(val.str, val.len);
    if (strVal == "true" || strVal == "false")
    {
        return strVal;
    }

    int64_t intVal = 0;
    if (ryml::from_chars(val, &intVal))
    {
        return std::to_string(intVal);
    }

    double doubleVal = 0.0;
    if (ryml::from_chars(val, &doubleVal))
    {
        return std::to_string(doubleVal);
    }

    return '"' + Luau::escape(strVal) + '"';
}
