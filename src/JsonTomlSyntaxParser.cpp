#include "LSP/JsonTomlSyntaxParser.hpp"
#include "Luau/StringUtils.h"

std::string jsonValueToLuau(const nlohmann::json& val)
{
    if (val.is_string() || val.is_number() || val.is_boolean())
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
            out += "[\"" + key + "\"] = ";
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
