#include <limits>
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "Luau/Common.h"

LUAU_FASTFLAG(LuauSolverV2)

static bool skipFastFlag(const char* flagName)
{
    if (strncmp(flagName, "Test", 4) == 0)
        return true;

    if (strncmp(flagName, "Debug", 5) == 0)
        return true;

    if (strcmp(flagName, "StudioReportLuauAny2") == 0)
        return true;

    return false;
}

template<typename T>
using FValueResult = std::pair<std::string, T>;

static FValueResult<std::optional<std::string>> parseFValueHelper(std::string_view view)
{
    size_t equalpos = view.find('=');
    if (equalpos == std::string_view::npos)
        return {std::string{view}, std::nullopt};

    std::string name{view.substr(0, equalpos)};
    view.remove_prefix(equalpos + 1);
    return {name, std::string{view}};
}

static FValueResult<int> parseFInt(std::string_view view)
{
    // If this was an FInt but there were no provided value, we should be noisy about that.
    // std::stoi already throws an exception on invalid conversion.
    auto [name, value] = parseFValueHelper(view);
    if (!value)
        throw std::runtime_error("Expected a value associated with " + name);

    return {name, std::stoi(*value)};
}

static FValueResult<bool> parseFFlag(std::string_view view)
{
    // If we have a flag name but there's no provided value, we default to true.
    auto [name, value] = parseFValueHelper(view);
    bool state = value ? *value == "true" : true;
    if (value && value != "true" && value != "false")
        fprintf(stderr, "Ignored '%s' because '%s' is not a valid flag state\n", name.c_str(), value->c_str());

    return {name, state};
}

template<typename T>
static void setFastValue(const std::string& name, T value)
{
    for (Luau::FValue<T>* fvalue = Luau::FValue<T>::list; fvalue; fvalue = fvalue->next)
        if (fvalue->name == name)
            fvalue->value = value;
}

static void setFastFlags(const std::vector<doctest::String>& flags)
{
    for (const doctest::String& flag : flags)
    {
        std::string_view view = flag.c_str();
        if (view == "true" || view == "false")
        {
            for (Luau::FValue<bool>* flag = Luau::FValue<bool>::list; flag; flag = flag->next)
            {
                if (!skipFastFlag(flag->name))
                    flag->value = view == "true";
            }

            continue;
        }

        if (view.size() >= 2 && view[0] == 'D' && view[1] == 'F')
            view.remove_prefix(1);

        if (view.substr(0, 4) == "FInt")
        {
            auto [name, value] = parseFInt(view.substr(4));
            setFastValue(name, value);
        }
        else
        {
            // We want to prevent the footgun where '--fflags=LuauSomeFlag' is ignored. We'll assume that this was declared as FFlag.
            auto [name, value] = parseFFlag(view.substr(0, 5) == "FFlag" ? view.substr(5) : view);
            setFastValue(name, value);
        }
    }
}

int main(int argc, const char** argv)
{
    doctest::Context context;
    context.applyCommandLine(argc, argv);

    if (std::vector<doctest::String> flags; doctest::parseCommaSepArgs(argc, argv, "--fflags=", flags))
        setFastFlags(flags);

    if (doctest::parseFlag(argc, argv, "--new-solver"))
    {
        FFlag::LuauSolverV2.value = true;
    }

    int test_result = context.run(); // run queries, or run tests unless --no-run

    if (context.shouldExit()) // honor query flags and --exit
        return test_result;

    return test_result; // combine the 2 results
}