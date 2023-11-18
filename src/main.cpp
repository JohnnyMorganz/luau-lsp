#include "LSP/LanguageServer.hpp"
#include "Analyze/AnalyzeCli.hpp"
#include "Luau/ExperimentalFlags.h"
#include "argparse/argparse.hpp"

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

LUAU_FASTINT(LuauTarjanChildLimit)

static void displayHelp(const char* argv0)
{
    printf("Usage: %s lsp [options]\n", argv0);
    printf("Usage: %s analyze [--mode] [options] [file list]\n", argv0);
    printf("\n");
    printf("Analyze options:\n");
    printf("  --formatter=plain: report analysis errors in Luacheck-compatible format\n");
    printf("  --formatter=gnu: report analysis errors in GNU-compatible format\n");
    printf("  --timetrace: record compiler time tracing information into trace.json\n");
    printf("  --no-strict-dm-types: disable strict DataModel types in type-checking\n");
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

int startLanguageServer(const argparse::ArgumentParser& program)
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    auto definitionsFiles = program.get<std::vector<std::filesystem::path>>("--definitions");
    auto documentationFiles = program.get<std::vector<std::filesystem::path>>("--docs");
    std::optional<std::filesystem::path> baseLuaurc = program.present<std::filesystem::path>("--base-luaurc");

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

std::filesystem::path file_path_parser(const std::string& value)
{
    return {value};
}

int main(int argc, char** argv)
{
    Luau::assertHandler() = [](const char* expr, const char* file, int line, const char*) -> int
    {
        fprintf(stderr, "%s(%d): ASSERTION FAILED: %s\n", file, line, expr);
        return 1;
    };

    argparse::ArgumentParser program("luau-lsp", "1.25.0");
    program.set_assign_chars(":=");

    // Global arguments
    program.add_argument("--delay-startup")
        .help("debug flag to halt startup to allow connection of a debugger")
        .default_value(false)
        .implicit_value(true);

    // FFlags
    program.add_argument("--flag").help("set a Luau FFlag. Syntax: `--flag:KEY=VALUE`").default_value<std::vector<std::string>>({}).append();
    program.add_argument("--no-flags-enabled").help("do not enable all Luau FFlags by default").default_value(false).implicit_value(true);
    program.add_argument("--show-flags")
        .help("display all the currently available Luau FFlags and their values")
        .default_value(false)
        .implicit_value(true);

    // Analyze arguments
    argparse::ArgumentParser analyze_command("analyze");
    analyze_command.add_description("Run luau-analyze type checking and linting");
    analyze_command.add_argument("--annotate")
        .help("output the source file with type annotations after typechecking")
        .default_value(false)
        .implicit_value(true);
    analyze_command.add_argument("--timetrace")
        .help("record compiler time tracing information into trace.json")
        .default_value(false)
        .implicit_value(true);
    analyze_command.add_argument("--formatter")
        .help("output analysis errors in a particular format")
        .default_value(std::string("default"))
        .choices("default", "plain", "gnu");
    analyze_command.add_argument("--no-strict-dm-types")
        .help("disable strict DataModel types in type-checking")
        .default_value(false)
        .implicit_value(true);
    analyze_command.add_argument("--sourcemap")
        .help("path to a Rojo-style instance sourcemap to understand the DataModel")
        .action(file_path_parser)
        .metavar("PATH");
    analyze_command.add_argument("--definitions")
        .help("A path to a Luau definitions file to load into the global namespace")
        .default_value<std::vector<std::filesystem::path>>({})
        .append()
        .metavar("PATH");
    analyze_command.add_argument("--ignore")
        .help("file glob pattern for ignoring error outputs")
        .default_value<std::vector<std::string>>({})
        .append()
        .metavar("GLOB");
    analyze_command.add_argument("--base-luaurc")
        .help("path to a .luaurc file which acts as the base default configuration")
        .action(file_path_parser)
        .metavar("PATH");
    analyze_command.add_argument("--settings").help("path to LSP-style settings").action(file_path_parser).metavar("PATH");
    analyze_command.add_argument("files").help("files to perform analysis on").remaining();

    // Language server arguments
    argparse::ArgumentParser lsp_command("lsp");
    lsp_command.add_description("Start the language server");
    lsp_command.add_epilog("This will start up a server which listens to LSP messages on stdin, and responds on stdout");
    lsp_command.add_argument("--definitions")
        .help("path to a Luau definitions file to load into the global namespace")
        .default_value<std::vector<std::filesystem::path>>({})
        .append()
        .metavar("PATH");
    lsp_command.add_argument("--docs", "--documentation")
        .help("path to a Luau documentation database for loaded definitions")
        .default_value<std::vector<std::filesystem::path>>({})
        .append()
        .metavar("PATH");
    lsp_command.add_argument("--base-luaurc")
        .help("path to a .luaurc file which acts as the base default configuration")
        .action(file_path_parser)
        .metavar("PATH");

    program.add_subparser(analyze_command);
    program.add_subparser(lsp_command);

    try
    {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err)
    {
        std::cerr << err.what() << '\n';
        std::cerr << program;
        return 1;
    }

    // Debug loop: set a breakpoint inside while loop to attach debugger before init
    if (program.is_used("--delay-startup"))
    {
        auto d = 4;
        while (d == 4)
        {
            d = 4;
        }
    }

    // Handle enabling FFlags
    bool enableAllFlags = !program.is_used("--no-flags-enabled");
    std::unordered_map<std::string, std::string> fastFlags{};
    for (const auto& flag : program.get<std::vector<std::string>>("--flag"))
    {
        size_t eqIndex = flag.find('=');
        if (eqIndex == std::string::npos)
        {
            std::cerr << "Invalid key-value pair, expected KEY=VALUE, instead got " << flag << '\n';
            return 1;
        }

        std::string flagName = flag.substr(0, eqIndex);
        std::string flagValue = flag.substr(eqIndex + 1, flag.length());
        fastFlags.emplace(flagName, flagValue);
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

    if (program.is_used("--show-flags"))
    {
        displayFlags();
        return 0;
    }

    if (program.is_subcommand_used("lsp"))
        return startLanguageServer(lsp_command);
    else if (program.is_subcommand_used("analyze"))
        return startAnalyze(analyze_command);

    // No sub-command specified
    std::cerr << "Specify a particular mode to run the program (analyze/lsp)" << '\n';
    std::cerr << program;
    return 1;
}
