#include "Flags.hpp"
#include "LSP/LanguageServer.hpp"
#include "LSP/DocumentationParser.hpp"
#include "Analyze/AnalyzeCli.hpp"
#include "Analyze/CliConfigurationParser.hpp"
#include "Luau/ExperimentalFlags.h"
#include "argparse/argparse.hpp"

#include "LSP/Transport/StdioTransport.hpp"
#ifndef _WIN32
#include "LSP/Transport/PipeTransport.hpp"
#endif

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#endif

#ifdef LSP_BUILD_WITH_SENTRY
// sentry.h pulls in <windows.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#endif
#define SENTRY_BUILD_STATIC 1
#include <sentry.h>
#endif

#define LSP_VERSION "1.45.0"

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

int startLanguageServer(const argparse::ArgumentParser& program)
{
#ifdef LSP_BUILD_WITH_SENTRY
    bool isCrashReportingEnabled = program.is_used("--enable-crash-reporting");
    if (isCrashReportingEnabled)
    {
        std::optional<std::filesystem::path> crashReportDirectory = program.present<std::filesystem::path>("--crash-report-directory");

        sentry_options_t* options = sentry_options_new();
        sentry_options_set_dsn(options, "https://bc658c75485d1aecbaf1c0c1f7980922@o4509305213026304.ingest.de.sentry.io/4509305221283920");

        if (crashReportDirectory.has_value())
        {
#ifdef _WIN32
            sentry_options_set_database_pathw(options, crashReportDirectory->c_str());
#else
            sentry_options_set_database_path(options, crashReportDirectory->c_str());
#endif
        }

        sentry_options_set_release(options, "luau-lsp@" LSP_VERSION);
        sentry_init(options);
    }
#endif

    // Debug loop: set a breakpoint inside while loop to attach debugger before init
    if (program.is_used("--delay-startup"))
    {
        auto d = 4;
        while (d == 4)
        {
            d = 4;
        }
    }

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    auto definitionsFiles = program.get<std::vector<std::filesystem::path>>("--definitions");
    auto documentationFiles = program.get<std::vector<std::filesystem::path>>("--docs");
    std::optional<std::filesystem::path> baseLuaurc = program.present<std::filesystem::path>("--base-luaurc");
    std::optional<std::filesystem::path> transportPipeFile = program.present<std::filesystem::path>("--pipe");

    std::optional<Luau::Config> defaultConfig = std::nullopt;
    if (baseLuaurc)
    {
        if (std::optional<std::string> contents = readFile(*baseLuaurc))
        {
            defaultConfig = Luau::Config{};
            std::optional<std::string> error = WorkspaceFileResolver::parseConfig(*baseLuaurc, *contents, *defaultConfig);
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

    // Setup client
    std::unique_ptr<Transport> transport;
    if (transportPipeFile)
    {
        if (program.is_used("--stdio"))
        {
            std::cerr << "both --stdio and --pipe cannot be specified at the same time\n";
            return 1;
        }
#ifdef _WIN32
        std::cerr << "--pipe is not supported on windows\n";
        return 1;
#else
        transport = std::make_unique<PipeTransport>(transportPipeFile->string());
#endif
    }
    else
    {
        transport = std::make_unique<StdioTransport>();
    }

    auto client = std::make_shared<Client>(std::move(transport));
    client->definitionsFiles = definitionsFiles;
    client->documentationFiles = documentationFiles;
    parseDocumentation(documentationFiles, client->documentation, client);

    // Parse LSP Settings
    auto settingsPath = program.present<std::filesystem::path>("--settings");
    if (settingsPath)
    {
        if (std::optional<std::string> contents = readFile(*settingsPath))
            client->globalConfig = dottedToClientConfiguration(contents.value());
        else
        {
            std::cerr << "Failed to read base LSP settings at '" << settingsPath->generic_string() << "'\n";
            return 1;
        }
    }

    LanguageServer server(client, defaultConfig);

    // Begin input loop
    server.processInputLoop();

#ifdef LSP_BUILD_WITH_SENTRY
    if (isCrashReportingEnabled)
        sentry_close();
#endif

    // If we received a shutdown request before exiting, exit normally. Otherwise, it is an abnormal exit
    return server.requestedShutdown() ? 0 : 1;
}

void processFFlags(const argparse::ArgumentParser& program)
{
    // Handle enabling FFlags
    bool enableAllFlags = !program.is_used("--no-flags-enabled");
    std::unordered_map<std::string, std::string> fastFlags{};
    for (const auto& flag : program.get<std::vector<std::string>>("--flag"))
    {
        size_t eqIndex = flag.find('=');
        if (eqIndex == std::string::npos)
        {
            std::cerr << "Invalid key-value pair, expected KEY=VALUE, instead got " << flag << '\n';
            exit(1);
        }

        std::string flagName = flag.substr(0, eqIndex);
        std::string flagValue = flag.substr(eqIndex + 1, flag.length());
        fastFlags.emplace(flagName, flagValue);
    }

    if (enableAllFlags)
    {
        for (Luau::FValue<bool>* flag = Luau::FValue<bool>::list; flag; flag = flag->next)
            if (strncmp(flag->name, "Luau", 4) == 0 && !Luau::isAnalysisFlagExperimental(flag->name))
                flag->value = true;
    }
    registerFastFlagsCLI(fastFlags);
    applyRequiredFlags();
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

    argparse::ArgumentParser program("luau-lsp", LSP_VERSION);
    program.set_assign_chars(":=");

    // Global arguments
    argparse::ArgumentParser parent_parser("-", "0.0", argparse::default_arguments::none);

    // FFlags
    parent_parser.add_argument("--flag")
        .help("set a Luau FFlag. Syntax: `--flag:KEY=VALUE`")
        .default_value<std::vector<std::string>>({})
        .append()
        .metavar("KEY=VALUE");
    parent_parser.add_argument("--no-flags-enabled").help("do not enable all Luau FFlags by default").default_value(false).implicit_value(true);
    program.add_argument("--show-flags")
        .help("display all the currently available Luau FFlags and their values")
        .default_value(false)
        .implicit_value(true);

    // Analyze arguments
    argparse::ArgumentParser analyze_command("analyze");
    analyze_command.add_description("Run luau-analyze type checking and linting");
    analyze_command.add_parents(parent_parser);
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
    analyze_command.add_argument("--definitions", "--defs")
        .help("A path to a Luau definitions file to load into the global namespace")
        .action(file_path_parser)
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
    analyze_command.add_argument("--platform").help("platform-specific support features").choices("standard", "roblox");
    analyze_command.add_argument("--settings").help("path to LSP-style settings").action(file_path_parser).metavar("PATH");
    analyze_command.add_argument("files").help("files to perform analysis on").remaining();

    // Language server arguments
    argparse::ArgumentParser lsp_command("lsp");
    lsp_command.add_description("Start the language server");
    lsp_command.add_epilog("This will start up a server which listens to LSP messages on stdin, and responds on stdout");
    lsp_command.add_parents(parent_parser);
    lsp_command.add_argument("--definitions")
        .help("path to a Luau definitions file to load into the global namespace")
        .action(file_path_parser)
        .default_value<std::vector<std::filesystem::path>>({})
        .append()
        .metavar("PATH");
    lsp_command.add_argument("--docs", "--documentation")
        .help("path to a Luau documentation database for loaded definitions")
        .action(file_path_parser)
        .default_value<std::vector<std::filesystem::path>>({})
        .append()
        .metavar("PATH");
    lsp_command.add_argument("--base-luaurc")
        .help("path to a .luaurc file which acts as the base default configuration")
        .action(file_path_parser)
        .metavar("PATH");
    lsp_command.add_argument("--settings").help("path to LSP settings to use as default").action(file_path_parser).metavar("PATH");
    lsp_command.add_argument("--delay-startup")
        .help("debug flag to halt startup to allow connection of a debugger")
        .default_value(false)
        .implicit_value(true);
    lsp_command.add_argument("--stdio").help("set up client communication channel via stdio").implicit_value(true);
    lsp_command.add_argument("--pipe")
        .help("path to pipe / socket file name for pipe communication channel")
        .action(file_path_parser)
        .metavar("PATH");
    lsp_command.add_argument("--enable-crash-reporting")
        .help("whether to enable crash reporting to Sentry")
        .default_value(false)
        .implicit_value(true);
    lsp_command.add_argument("--crash-report-directory")
        .help("location to store database for crash reports")
        .action(file_path_parser)
        .metavar("PATH");

    program.add_parents(parent_parser);
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

    processFFlags(program);
    if (program.is_used("--show-flags"))
    {
        displayFlags();
        return 0;
    }

    if (program.is_subcommand_used("lsp"))
    {
        processFFlags(lsp_command);
        return startLanguageServer(lsp_command);
    }
    else if (program.is_subcommand_used("analyze"))
    {
        processFFlags(analyze_command);
        return startAnalyze(analyze_command);
    }

    // No sub-command specified
    std::cerr << "Specify a particular mode to run the program (analyze/lsp)" << '\n';
    std::cerr << program;
    return 1;
}
