#include "LSP/LanguageServer.hpp"

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

void registerFastFlags(std::unordered_map<std::string, std::string>& fastFlags)
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
                std::cerr << "Bad flag option, expected a boolean 'True' or 'False' for flag " << flag->name << "\n";
                std::exit(1);
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
                std::cerr << "Bad flag option, expected an int for flag " << flag->name << "\n";
                std::exit(1);
            }

            flag->value = value;
            fastFlags.erase(flag->name);
        }
    }

    for (auto& [key, _] : fastFlags)
    {
        std::cerr << "Unknown FFlag: " << key << "\n";
    }
}

int main(int argc, char** argv)
{
    // Debug loop: uncomment and set a breakpoint on while to attach debugger before init
    // auto d = 4;
    // while (d == 4)
    // {
    //     d = 4;
    // }

    Luau::assertHandler() = [](const char* expr, const char* file, int line, const char*) -> int
    {
        fprintf(stderr, "%s(%d): ASSERTION FAILED: %s\n", file, line, expr);
        return 1;
    };

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    // Enable all flags
    for (Luau::FValue<bool>* flag = Luau::FValue<bool>::list; flag; flag = flag->next)
        if (strncmp(flag->name, "Luau", 4) == 0)
            flag->value = true;

    // Check passed arguments
    std::optional<std::filesystem::path> definitionsFile;
    std::optional<std::filesystem::path> documentationFile;
    std::unordered_map<std::string, std::string> fastFlags;
    for (int i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--definitions=", 14) == 0)
        {
            definitionsFile = std::filesystem::path(argv[i] + 14);
        }
        else if (strncmp(argv[i], "--docs=", 7) == 0)
        {
            documentationFile = std::filesystem::path(argv[i] + 7);
        }
        else if (strncmp(argv[i], "--flag:", 7) == 0)
        {
            std::string flagSet = std::string(argv[i] + 7);

            size_t eqIndex = flagSet.find("=");
            if (eqIndex == std::string::npos)
            {
                std::cerr << "Bad flag option, missing =: " << flagSet << "\n";
                return 1;
            }

            std::string flagName = flagSet.substr(0, eqIndex);
            std::string flagValue = flagSet.substr(eqIndex + 1, flagSet.length());
            fastFlags.emplace(flagName, flagValue);
        }
    }
    registerFastFlags(fastFlags);

    LanguageServer server(definitionsFile, documentationFile);

    // Begin input loop
    server.processInputLoop();

    // If we received a shutdown request before exiting, exit normally. Otherwise, it is an abnormal exit
    return server.requestedShutdown() ? 0 : 1;
}