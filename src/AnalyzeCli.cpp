// Source: https://github.com/Roblox/luau/blob/master/CLI/Analyze.cpp; licensed under MIT License
#include "Analyze/AnalyzeCli.hpp"
#include "Analyze/CliConfigurationParser.hpp"
#include "Analyze/CliClient.hpp"
#include "Flags.hpp"

#include "LSP/ClientConfiguration.hpp"
#include "Platform/LSPPlatform.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/Frontend.h"
#include "Luau/TypeAttach.h"
#include "Luau/Transpiler.h"
#include "LuauFileUtils.hpp"
#include "LSP/LuauExt.hpp"
#include "LSP/WorkspaceFileResolver.hpp"
#include "glob/match.h"
#include <iostream>
#include <memory>
#include <vector>

LUAU_FASTFLAG(DebugLuauTimeTracing)
LUAU_FASTFLAG(LuauSolverV2)

enum class ReportFormat
{
    Default,
    Luacheck,
    Gnu,
};

static void report(ReportFormat format, const char* name, const Luau::Location& loc, const char* type, const char* message)
{
    switch (format)
    {
    case ReportFormat::Default:
        fprintf(stderr, "%s(%d,%d): %s: %s\n", name, loc.begin.line + 1, loc.begin.column + 1, type, message);
        break;

    case ReportFormat::Luacheck:
    {
        // Note: luacheck's end column is inclusive but our end column is exclusive
        // In addition, luacheck doesn't support multi-line messages, so if the error is multiline we'll fake end column as 100 and hope for the best
        unsigned int columnEnd = (loc.begin.line == loc.end.line) ? loc.end.column : 100;

        // Use stdout to match luacheck behavior
        fprintf(stdout, "%s:%d:%d-%d: (W0) %s: %s\n", name, loc.begin.line + 1, loc.begin.column + 1, columnEnd, type, message);
        break;
    }

    case ReportFormat::Gnu:
        // Note: GNU end column is inclusive but our end column is exclusive
        fprintf(stderr, "%s:%d.%d-%d.%d: %s: %s\n", name, loc.begin.line + 1, loc.begin.column + 1, loc.end.line + 1, loc.end.column, type, message);
        break;
    }
}

static bool isIgnoredFile(const Uri& rootUri, const Uri& uri, const std::vector<std::string>& ignoreGlobPatterns)
{
    // We want to test globs against a relative path to workspace, since that's what makes most sense
    auto relativePathString = uri.lexicallyRelative(rootUri);
    for (auto& pattern : ignoreGlobPatterns)
    {
        if (glob::gitignore_glob_match(relativePathString, pattern))
            return true;
    }
    return false;
}

FilePathInformation getFilePath(const WorkspaceFileResolver* fileResolver, const Luau::ModuleName& moduleName)
{
    auto path = fileResolver->platform->resolveToRealPath(moduleName);
    LUAU_ASSERT(path);

    // For consistency, we want to map the error.moduleName to a relative path (if it is a real path)
    Luau::ModuleName errorFriendlyName = moduleName;
    if (!fileResolver->platform->isVirtualPath(moduleName))
        errorFriendlyName = path->lexicallyRelative(fileResolver->rootUri);

    return {*path, fileResolver->getHumanReadableModuleName(errorFriendlyName)};
}

static bool reportError(
    const Luau::Frontend& frontend, ReportFormat format, const Luau::TypeError& error, std::vector<std::string>& ignoreGlobPatterns)
{
    auto* fileResolver = static_cast<WorkspaceFileResolver*>(frontend.fileResolver);
    auto [uri, relativePath] = getFilePath(fileResolver, error.moduleName);

    if (isIgnoredFile(fileResolver->rootUri, uri, ignoreGlobPatterns))
        return false;

    if (const auto* syntaxError = Luau::get_if<Luau::SyntaxError>(&error.data))
        report(format, relativePath.c_str(), error.location, "SyntaxError", syntaxError->message.c_str());
    else
        report(format, relativePath.c_str(), error.location, "TypeError",
            Luau::toString(error, Luau::TypeErrorToStringOptions{frontend.fileResolver}).c_str());

    return true;
}

static void reportWarning(ReportFormat format, const char* name, const Luau::LintWarning& warning)
{
    report(format, name, warning.location, Luau::LintWarning::getName(warning.code), warning.text.c_str());
}

static bool analyzeFile(
    Luau::Frontend& frontend, const std::string& path, ReportFormat format, bool annotate, std::vector<std::string>& ignoreGlobPatterns)
{
    Luau::CheckResult cr;
    Luau::ModuleName name = path;

    if (frontend.isDirty(name))
        cr = frontend.check(name);

    if (!frontend.getSourceModule(name))
    {
        std::cerr << "Error opening " << name << "\n";
        return false;
    }

    unsigned int reportedErrors = 0;
    for (auto& error : cr.errors)
        reportedErrors += reportError(frontend, format, error, ignoreGlobPatterns);

    // For the human readable module name, we use a relative version
    auto [_, relativePath] = getFilePath(static_cast<WorkspaceFileResolver*>(frontend.fileResolver), path);
    for (auto& error : cr.lintResult.errors)
        reportWarning(format, relativePath.c_str(), error);
    for (auto& warning : cr.lintResult.warnings)
        reportWarning(format, relativePath.c_str(), warning);

    if (annotate)
    {
        Luau::SourceModule* sm = frontend.getSourceModule(name);
        Luau::ModulePtr m = frontend.moduleResolver.getModule(name);

        Luau::attachTypeData(*sm, *m);

        std::string annotated = Luau::transpileWithTypes(*sm->root);

        printf("%s", annotated.c_str());
    }

    return reportedErrors == 0 && cr.lintResult.errors.empty();
}

std::vector<std::string> getFilesToAnalyze(const std::vector<std::string>& paths, const std::vector<std::string>& ignoreGlobPatterns)
{
    auto cwd = Luau::FileUtils::getCurrentWorkingDirectory();
    LUAU_ASSERT(cwd);
    auto cwdUri = Uri::file(*cwd);

    std::vector<std::string> files;
    for (const auto& pathString : paths)
    {
        Uri uri = cwdUri.resolvePath(pathString);
        if (!uri.exists())
        {
            std::cerr << "Cannot get " << uri.fsPath() << ": path does not exist\n";
            exit(1);
        }


        if (uri.isDirectory())
        {
            Luau::FileUtils::traverseDirectoryRecursive(uri.fsPath(),
                [&](const auto& path)
                {
                    auto uri = Uri::file(path);
                    auto ext = uri.extension();
                    if (ext == ".lua" || ext == ".luau")
                    {
                        if (!isIgnoredFile(cwdUri, uri, ignoreGlobPatterns))
                            files.push_back(uri.fsPath());
                    }
                });
        }
        else
        {
            files.push_back(uri.fsPath());
        }
    }
    return files;
}

void applySettings(
    const std::string& settingsContents, CliClient& client, std::vector<std::string>& ignoreGlobPatterns, std::vector<std::string>& definitionsPaths)
{
    client.configuration = dottedToClientConfiguration(settingsContents);

    auto& ignoreGlobsConfiguration = client.configuration.ignoreGlobs;
    auto& definitionsFilesConfiguration = client.configuration.types.definitionFiles;

    ignoreGlobPatterns.reserve(ignoreGlobPatterns.size() + ignoreGlobsConfiguration.size());
    ignoreGlobPatterns.insert(ignoreGlobPatterns.end(), ignoreGlobsConfiguration.cbegin(), ignoreGlobsConfiguration.cend());
    definitionsPaths.reserve(definitionsPaths.size() + definitionsFilesConfiguration.size());
    definitionsPaths.insert(definitionsPaths.end(), definitionsFilesConfiguration.cbegin(), definitionsFilesConfiguration.cend());

    // Process any fflags
    registerFastFlagsCLI(client.configuration.fflags.override);
    if (!client.configuration.fflags.enableByDefault)
        std::cerr << "warning: `luau-lsp.fflags.enableByDefault` is not respected in CLI Analyze mode. Please instead use the CLI option "
                     "`--no-flags-enabled` to configure this.\n";
    if (client.configuration.fflags.sync)
        std::cerr << "warning: `luau-lsp.fflags.sync` is not supported in CLI Analyze mode. Instead, all FFlags are enabled by default. "
                     "Please manually configure necessary FFlags\n";
}

int startAnalyze(const argparse::ArgumentParser& program)
{
    ReportFormat format = ReportFormat::Default;
    bool annotate = program.is_used("--annotate");
    auto sourcemapPath = program.present<std::string>("--sourcemap");
    auto definitionsPaths = program.get<std::vector<std::string>>("--definitions");
    auto ignoreGlobPatterns = program.get<std::vector<std::string>>("--ignore");
    auto baseLuaurc = program.present<std::string>("--base-luaurc");
    auto settingsPath = program.present<std::string>("--settings");
    std::vector<std::string> files{};
    FFlag::DebugLuauTimeTracing.value = program.is_used("--timetrace");

    CliClient client;

    auto currentWorkingDirectory = Luau::FileUtils::getCurrentWorkingDirectory();
    if (!currentWorkingDirectory)
    {
        fprintf(stderr, "Failed to determine current working directory\n");
        return 1;
    }

    if (settingsPath)
    {
        if (std::optional<std::string> contents = Luau::FileUtils::readFile(*settingsPath))
        {
            applySettings(contents.value(), client, ignoreGlobPatterns, definitionsPaths);
        }
        else
        {
            fprintf(stderr, "Failed to read settings at '%s'\n", settingsPath->c_str());
            return 1;
        }
    }

    if (auto filesArg = program.present<std::vector<std::string>>("files"))
        files = getFilesToAnalyze(*filesArg, ignoreGlobPatterns);

    if (files.empty())
    {
        fprintf(stderr, "error: no files provided\n");
        return 1;
    }

    auto reportFormatter = program.get<std::string>("--formatter");
    if (reportFormatter == "default")
        format = ReportFormat::Default;
    else if (reportFormatter == "plain")
        format = ReportFormat::Luacheck;
    else if (reportFormatter == "gnu")
        format = ReportFormat::Gnu;

#if !defined(LUAU_ENABLE_TIME_TRACE)
    if (FFlag::DebugLuauTimeTracing)
    {
        printf("To run with --timetrace, Luau has to be built with LUAU_ENABLE_TIME_TRACE enabled\n");
        return 1;
    }
#endif

    if (files.empty())
    {
        fprintf(stderr, "error: no files provided\n");
        return 1;
    }

    // Setup Frontend
    Luau::FrontendOptions frontendOptions;
    frontendOptions.retainFullTypeGraphs = annotate;
    frontendOptions.runLintChecks = true;

    WorkspaceFileResolver fileResolver;
    if (baseLuaurc)
    {
        Luau::Config result;
        if (std::optional<std::string> contents = Luau::FileUtils::readFile(*baseLuaurc))
        {
            std::optional<std::string> error = WorkspaceFileResolver::parseConfig(Uri::file(*baseLuaurc), *contents, result);
            if (error)
            {
                fprintf(stderr, "%s: %s\n", baseLuaurc->c_str(), error->c_str());
                return 1;
            }
            fileResolver.defaultConfig = std::move(result);
        }
        else
        {
            fprintf(stderr, "Failed to read base .luaurc configuration at '%s'\n", baseLuaurc->c_str());
            return 1;
        }
    }

    fileResolver.rootUri = Uri::file(*currentWorkingDirectory);
    fileResolver.client = &client;

    if (auto platformArg = program.present("--platform"))
    {
        if (platformArg == "standard")
            client.configuration.platform.type = LSPPlatformConfig::Standard;
        else if (platformArg == "roblox")
            client.configuration.platform.type = LSPPlatformConfig::Roblox;
    }

    std::unique_ptr<LSPPlatform> platform = LSPPlatform::getPlatform(client.configuration, &fileResolver);

    fileResolver.platform = platform.get();
    fileResolver.requireSuggester = fileResolver.platform->getRequireSuggester();

    Luau::Frontend frontend(&fileResolver, &fileResolver, frontendOptions);

    Luau::registerBuiltinGlobals(frontend, frontend.globals);
    if (!FFlag::LuauSolverV2)
        Luau::registerBuiltinGlobals(frontend, frontend.globalsForAutocomplete);

    if (client.configuration.platform.type == LSPPlatformConfig::Roblox && definitionsPaths.empty())
    {
        fprintf(stderr, "WARNING: --platform is set to 'roblox' but no definitions files are provided. 'luau-lsp analyze' does not download "
                        "definitions files; use `--platform=standard` to silence\n");
    }

    for (auto& definitionsPath : definitionsPaths)
    {
        auto uri = fileResolver.rootUri.resolvePath(definitionsPath);
        if (!uri.exists())
        {
            fprintf(stderr, "Cannot load definitions file %s: path does not exist\n", definitionsPath.c_str());
            return 1;
        }

        auto definitionsContents = Luau::FileUtils::readFile(uri.fsPath());
        if (!definitionsContents)
        {
            fprintf(stderr, "Cannot load definitions file %s: failed to read\n", definitionsPath.c_str());
            return 1;
        }

        auto loadResult = types::registerDefinitions(frontend, frontend.globals, *definitionsContents);
        if (!loadResult.success)
        {
            fprintf(stderr, "Failed to load definitions\n");
            for (const auto& error : loadResult.parseResult.errors)
                report(format, uri.fsPath().c_str(), error.getLocation(), "SyntaxError", error.getMessage().c_str());

            if (loadResult.module)
            {
                for (const auto& error : loadResult.module->errors)
                    if (const auto* syntaxError = Luau::get_if<Luau::SyntaxError>(&error.data))
                        report(format, uri.fsPath().c_str(), error.location, "SyntaxError", syntaxError->message.c_str());
                    else
                        report(format, uri.fsPath().c_str(), error.location, "TypeError", Luau::toString(error).c_str());
            }
            return 1;
        }

        platform->mutateRegisteredDefinitions(frontend.globals, types::parseDefinitionsFileMetadata(*definitionsContents));
    }

    if (sourcemapPath)
    {
        if (client.configuration.platform.type == LSPPlatformConfig::Roblox)
        {
            auto robloxPlatform = dynamic_cast<RobloxPlatform*>(platform.get());

            if (auto sourceMapContents = Luau::FileUtils::readFile(*sourcemapPath))
            {
                robloxPlatform->updateSourceNodeMap(sourceMapContents.value());

                bool expressiveTypes =
                    (program.is_used("--no-strict-dm-types") && client.configuration.diagnostics.strictDatamodelTypes) || FFlag::LuauSolverV2;
                robloxPlatform->handleSourcemapUpdate(frontend, frontend.globals, expressiveTypes);
            }
            else
            {
                fprintf(stderr, "Cannot load sourcemap path %s: failed to read contents\n", sourcemapPath->c_str());
                return 1;
            }
        }
        else
        {
            std::cerr << "warning: a sourcemap was provided, but the current platform is not `roblox`. Use `--platform roblox` to ensure the "
                         "sourcemap option is respected.\n";
        }
    }

    Luau::freeze(frontend.globals.globalTypes);
    if (!FFlag::LuauSolverV2)
        Luau::freeze(frontend.globalsForAutocomplete.globalTypes);

    int failed = 0;

    for (const auto& path : files)
        failed += !analyzeFile(frontend, path, format, annotate, ignoreGlobPatterns);

    if (!client.diagnostics.empty())
    {
        failed += int(client.diagnostics.size());

        for (const auto& [path, err] : client.diagnostics)
            fprintf(stderr, "%s: %s\n", path.fsPath().c_str(), err.c_str());
    }

    if (format == ReportFormat::Luacheck)
        return 0;
    else
        return failed ? 1 : 0;
}
