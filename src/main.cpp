#include "LSP/LanguageServer.hpp"
#include "Analyze/AnalyzeCli.hpp"
#include "Luau/ExperimentalFlags.h"

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

LUAU_FASTINT(LuauTarjanChildLimit)

enum class CliMode
{
    Lsp,
    Analyze
};

/// Validate to ensure the flag is correct
static bool validateFlag(char* str, int argIndex)
{
    // Ignore arguments not starting with "-"
    if (str[0] != '-')
        return true;

    auto n = strlen(str);

    if (argIndex == 1 && strcmp(str, "--help") == 0)
        return true;
    else if (argIndex == 1 && strcmp(str, "--show-flags") == 0)
        return true;
    else if (strncmp(str, "--flag:", 7) == 0 && n > 8)
        return true;
    else if (strcmp(str, "--no-flags-enabled") == 0)
        return true;
    else if (strncmp(str, "--definitions=", 14) == 0 && n > 15)
        return true;
    else if (strncmp(str, "--docs=", 7) == 0 && n > 8)
        return true;
    else if (strncmp(str, "--base-luaurc=", 14) == 0 && n > 15)
        return true;
    else if (strcmp(str, "--formatter=plain") == 0)
        return true;
    else if (strcmp(str, "--formatter=gnu") == 0)
        return true;
    else if (strcmp(str, "--annotate") == 0)
        return true;
    else if (strcmp(str, "--timetrace") == 0)
        return true;
    else if (strncmp(str, "--sourcemap=", 12) == 0 && n > 13)
        return true;
    else if (strncmp(str, "--defs=", 7) == 0 && n > 8)
        return true;
    else if (strncmp(str, "--ignore=", 9) == 0 && n > 10)
        return true;
    return false;
}

static void displayInvalidCommand(const std::string& argv0)
{
    std::cout << "Invalid command. You must specify a mode to run\n";
    std::cout << "Usage: " + argv0 + " lsp [options]\n";
    std::cout << "Usage: " + argv0 + " analyze [--mode] [options] [file list]\n";
    std::cout << "Run '" + argv0 + " --help' for more information\n";
}

static void displayHelp(const char* argv0)
{
    printf("Usage: %s lsp [options]\n", argv0);
    printf("Usage: %s analyze [--mode] [options] [file list]\n", argv0);
    printf("\n");
    printf("Global commands:\n");
    printf("  %s --help: show this help message\n", argv0);
    printf("  %s --show-flags: show all internal flags and their values\n", argv0);
    printf("\n");
    printf("Global options:\n");
    printf("  --no-flags-enabled: does not enable all internal FFlags.\n");
    printf("  --flag:FlagName=value: sets an internal flag. Use --show-flags to list all internal flags and default values.\n");
    printf("\n");
    printf("Analyze modes:\n");
    printf("  omitted: typecheck and lint input files\n");
    printf("  --annotate: typecheck input files and output source with type annotations\n");
    printf("\n");
    printf("Analyze options:\n");
    printf("  --formatter=plain: report analysis errors in Luacheck-compatible format\n");
    printf("  --formatter=gnu: report analysis errors in GNU-compatible format\n");
    printf("  --timetrace: record compiler time tracing information into trace.json\n");
    printf("  --sourcemap=PATH: path to a Rojo-style sourcemap\n");
    printf("  --definitions=PATH: path to definition file for global types\n");
    printf("  --ignore=GLOB: glob pattern to ignore error outputs\n");
    printf("  --base-luaurc=PATH: path to a .luaurc file which acts as the base default configuration\n");
    printf("LSP options:\n");
    printf("  --definitions=PATH: path to definition file for global types\n");
    printf("  --docs=PATH: path to documentation file to power Intellisense\n");
    printf("  --base-luaurc=PATH: path to a .luaurc file which acts as the base default configuration\n");
}

static void displayFlags()
{
    printf("Available flags:\n");

    for (Luau::FValue<bool>* flag = Luau::FValue<bool>::list; flag; flag = flag->next)
    {
        printf("  %s=%s\n", flag->name, flag->value ? "true" : "false");
    }

    for (Luau::FValue<int>* flag = Luau::FValue<int>::list; flag; flag = flag->next)
    {
        printf("  %s=%d\n", flag->name, flag->value);
    }
}

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

int startLanguageServer(int argc, char** argv)
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    std::vector<std::filesystem::path> definitionsFiles{};
    std::vector<std::filesystem::path> documentationFiles{};
    std::optional<std::filesystem::path> baseLuaurc = std::nullopt;

    for (int i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--definitions=", 14) == 0)
        {
            definitionsFiles.emplace_back(argv[i] + 14);
        }
        else if (strncmp(argv[i], "--docs=", 7) == 0)
        {
            documentationFiles.emplace_back(argv[i] + 7);
        }
        else if (strncmp(argv[i], "--base-luaurc=", 14) == 0)
        {
            baseLuaurc = std::filesystem::path(argv[i] + 14);
        }
    }

    std::optional<Luau::Config> defaultConfig = std::nullopt;
    if (baseLuaurc)
    {
        if (std::optional<std::string> contents = readFile(*baseLuaurc))
        {
            defaultConfig = Luau::Config{};
            std::optional<std::string> error = Luau::parseConfig(*contents, *defaultConfig);
            if (error)
            {
                std::cerr << baseLuaurc->generic_string() << ": " << *error << "\n";
                return 1;
            }
        }
        else
        {
            std::cerr << "Failed to read base .luaurc configuration at '" << baseLuaurc->generic_string() << "'\n";
            return 1;
        }
    }

    LanguageServer server(definitionsFiles, documentationFiles, defaultConfig);

    // Begin input loop
    server.processInputLoop();

    // If we received a shutdown request before exiting, exit normally. Otherwise, it is an abnormal exit
    return server.requestedShutdown() ? 0 : 1;
}

int main(int argc, char** argv)
{
    // Debug loop: uncomment and set a breakpoint on while to attach debugger before init
    // auto d = 4;
    // while (d == 4)
    // {
    //     d = 4;
    // }

    CliMode mode = CliMode::Lsp;

    if (argc < 2)
    {
        displayInvalidCommand(argv[0]);
        return 1;
    }
    else if (strcmp(argv[1], "--help") == 0)
    {
        displayHelp(argv[0]);
        return 0;
    }
    else if (strcmp(argv[1], "--show-flags") == 0)
    {
        displayFlags();
        return 0;
    }
    else if (strcmp(argv[1], "lsp") == 0)
    {
        mode = CliMode::Lsp;
    }
    else if (strcmp(argv[1], "analyze") == 0)
    {
        mode = CliMode::Analyze;
    }
    else
    {
        displayInvalidCommand(argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++)
    {
        if (!validateFlag(argv[i], i))
        {
            std::cerr << "Unknown option '" << std::string(argv[i]) << "'\n";
            std::cerr << "Run '" + std::string(argv[0]) + " --help' for more information\n";
            return 1;
        }
    }

    Luau::assertHandler() = [](const char* expr, const char* file, int line, const char*) -> int
    {
        fprintf(stderr, "%s(%d): ASSERTION FAILED: %s\n", file, line, expr);
        return 1;
    };

    // Handle enabling FFlags
    bool enableAllFlags = true;
    std::unordered_map<std::string, std::string> fastFlags{};
    for (int i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--flag:", 7) == 0)
        {
            std::string flagSet = std::string(argv[i] + 7);

            size_t eqIndex = flagSet.find('=');
            if (eqIndex == std::string::npos)
            {
                std::cerr << "Bad flag option, missing =: " << flagSet << "\n";
                return 1;
            }

            std::string flagName = flagSet.substr(0, eqIndex);
            std::string flagValue = flagSet.substr(eqIndex + 1, flagSet.length());
            fastFlags.emplace(flagName, flagValue);
        }
        else if (strcmp(argv[i], "--no-flags-enabled") == 0)
        {
            enableAllFlags = false;
        }
    }

    if (enableAllFlags)
    {
        for (Luau::FValue<bool>* flag = Luau::FValue<bool>::list; flag; flag = flag->next)
            if (strncmp(flag->name, "Luau", 4) == 0 && !Luau::isFlagExperimental(flag->name))
                flag->value = true;
    }
    registerFastFlags(fastFlags);

    // Manually enforce a LuauTarjanChildLimit increase
    // TODO: re-evaluate the necessity of this change
    if (FInt::LuauTarjanChildLimit > 0 && FInt::LuauTarjanChildLimit < 15000)
        FInt::LuauTarjanChildLimit.value = 15000;

    if (mode == CliMode::Lsp)
        return startLanguageServer(argc, argv);
    else
        return startAnalyze(argc, argv);
}