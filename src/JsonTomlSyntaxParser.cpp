#define RYML_SINGLE_HDR_DEFINE_NOW
#include "LSP/JsonTomlSyntaxParser.hpp"
#include "Luau/StringUtils.h"

static std::string stringValueToLuau(const std::string& value, const DataFileToLuauOptions& options)
{
    std::string escaped = '"' + Luau::escape(value) + '"';
    if (options.shouldUseStringSingleton && options.shouldUseStringSingleton(value))
        return "(" + escaped + " :: " + escaped + ")";
    return escaped;
}

std::string jsonValueToLuau(const nlohmann::json& val, const DataFileToLuauOptions& options)
{
    if (val.is_string())
    {
        return stringValueToLuau(val.get<std::string>(), options);
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
            out += jsonValueToLuau(elem, options);
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
            out += jsonValueToLuau(value, options);
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

std::string tomlValueToLuau(const toml::value& val, const DataFileToLuauOptions& options)
{
    if (val.is_string())
    {
        std::string str = val.as_string();
        return stringValueToLuau(str, options);
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
            out += tomlValueToLuau(elem, options);
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
            out += tomlValueToLuau(value, options);
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

std::string yamlValueToLuau(ryml::ConstNodeRef node, const DataFileToLuauOptions& options)
{
    if (node.is_seq())
    {
        std::string out = "{";
        for (const auto& child : node.children())
        {
            out += yamlValueToLuau(child, options);
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
            out += yamlValueToLuau(child, options);
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

    return stringValueToLuau(strVal, options);
}
