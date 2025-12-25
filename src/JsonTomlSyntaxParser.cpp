#define RYML_SINGLE_HDR_DEFINE_NOW
#include "LSP/JsonTomlSyntaxParser.hpp"
#include "Luau/StringUtils.h"

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

    if (val == "~" || val == "null" || val == "Null" || val == "NULL")
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
