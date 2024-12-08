/// This is used only in the CLI to parse vscode-style configuration into ClientConfiguration

#include "Luau/StringUtils.h"
#include "Analyze/CliConfigurationParser.hpp"

#include <iostream>

json parseDottedConfiguration(const std::string& contents)
{
    json data;

    try
    {
        data = json::parse(contents);
    }
    catch (const json::exception& err)
    {
        std::cerr << "Failed to parse settings JSON: " << err.what() << std::endl;
    }

    json output = json::object();


    for (const auto& el : data.items())
    {
        auto parts = Luau::split(el.key(), '.');

        // Remove the first part
        // TODO: we should warn if the first part is not "luau" or "luau-lsp"
        parts.erase(parts.begin());

        auto* current = &output;

        for (size_t i = 0; i < parts.size(); i++)
        {
            auto key = parts[i];
            if (i == parts.size() - 1)
            {
                current->emplace(key, el.value());
            }
            else
            {
                if (!current->contains(key))
                    current->emplace(key, json::object());
                current = &current->at(key);
            }
        }
    }

    return output;
}

ClientConfiguration dottedToClientConfiguration(const std::string& contents)
{
    return parseDottedConfiguration(contents);
}
