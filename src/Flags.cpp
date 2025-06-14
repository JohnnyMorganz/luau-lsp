#include "Flags.hpp"

#include "Luau/Common.h"

#include <iostream>

LUAU_FASTINT(LuauTarjanChildLimit)
LUAU_FASTINT(LuauTableTypeMaximumStringifierLength)

void registerFastFlags(std::unordered_map<std::string, std::string>& fastFlags, ErrorCallback onError, ErrorCallback onWarning)
{
    for (Luau::FValue<bool>* flag = Luau::FValue<bool>::list; flag; flag = flag->next)
    {
        if (fastFlags.find(flag->name) != fastFlags.end())
        {
            std::string valueStr = fastFlags.at(flag->name);

            if (valueStr == "true" || valueStr == "True")
                flag->value = true;
            else if (valueStr == "false" || valueStr == "False")
                flag->value = false;
            else
            {
                onError(std::string("Bad flag option, expected a boolean 'True' or 'False' for flag ") + flag->name);
            }

            fastFlags.erase(flag->name);
        }
    }

    for (Luau::FValue<int>* flag = Luau::FValue<int>::list; flag; flag = flag->next)
    {
        if (fastFlags.find(flag->name) != fastFlags.end())
        {
            std::string valueStr = fastFlags.at(flag->name);

            int value = 0;
            try
            {
                value = std::stoi(valueStr);
            }
            catch (...)
            {
                onError(std::string("Bad flag option, expected an int for flag ") + flag->name);
            }

            flag->value = value;
            fastFlags.erase(flag->name);
        }
    }

    for (auto& [key, _] : fastFlags)
    {
        onWarning(std::string("Unknown FFlag: ") + key);
    }
}

void registerFastFlagsCLI(std::unordered_map<std::string, std::string>& fastFlags)
{
    registerFastFlags(
        fastFlags,
        [](const std::string& message)
        {
            std::cerr << message << '\n';
            std::exit(1);
        },
        [](const std::string& message)
        {
            std::cerr << message << '\n';
        });
}

void applyRequiredFlags()
{
    // Manually enforce a LuauTarjanChildLimit increase
    // TODO: re-evaluate the necessity of this change
    if (FInt::LuauTarjanChildLimit > 0 && FInt::LuauTarjanChildLimit < 15000)
        FInt::LuauTarjanChildLimit.value = 15000;
    // This flag value is enforced due to Studio, but modern editors can handle this easily. Let's remove the restriction by setting it to zero.
    // NOTE: 40 is the current value on Studio. We check against that so that we don't inadvertently change people's overrides
    if (FInt::LuauTableTypeMaximumStringifierLength == 40)
        FInt::LuauTableTypeMaximumStringifierLength.value = 0;
}