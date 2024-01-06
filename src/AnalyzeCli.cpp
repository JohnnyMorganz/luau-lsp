// Source: https://github.com/Roblox/luau/blob/master/CLI/Analyze.cpp; licensed under MIT License
#include "Analyze/AnalyzeCli.hpp"
#include "Analyze/CliConfigurationParser.hpp"
#include "Analyze/CliClient.hpp"

#include "LSP/ClientConfiguration.hpp"
#include "Platform/LSPPlatform.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "Luau/ModuleResolver.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/Frontend.h"
#include "Luau/TypeAttach.h"
#include "Luau/Transpiler.h"
#include "LSP/LuauExt.hpp"
#include "LSP/WorkspaceFileResolver.hpp"
#include "LSP/Utils.hpp"
#include "glob/glob.hpp"
#include <iostream>
#include <filesystem>
#include <memory>
#include <vector>

LUAU_FASTFLAG(DebugLuauTimeTracing)

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

static bool isIgnoredFile(const std::filesystem::path& rootUriPath, const std::filesystem::path& path, std::vector<std::string>& ignoreGlobPatterns)
{
    auto relativePath = path.lexically_relative(rootUriPath).generic_string(); // HACK: we convert to generic string so we get '/' separators

    // luau analyze returns relative path for files that are to be analyzed
    if (relativePath.empty())
        relativePath = path.generic_string();

    for (auto& pattern : ignoreGlobPatterns)
        if (glob::fnmatch_case(relativePath, pattern))
            return true;

    return false;
}

static bool reportError(
    const Luau::Frontend& frontend, ReportFormat format, const Luau::TypeError& error, std::vector<std::string>& ignoreGlobPatterns)
{
    auto* fileResolver = static_cast<WorkspaceFileResolver*>(frontend.fileResolver);
    std::filesystem::path rootUriPath = fileResolver->rootUri.fsPath();
    auto path = fileResolver->platform->resolveToRealPath(error.moduleName);

    // For consistency, we want to map the error.moduleName to a relative path (if it is a real path)
    Luau::ModuleName errorFriendlyName = error.moduleName;
    if (!fileResolver->platform->isVirtualPath(error.moduleName))
        errorFriendlyName = std::filesystem::proximate(*path, rootUriPath).generic_string();

    std::string humanReadableName = fileResolver->getHumanReadableModuleName(errorFriendlyName);

    if (isIgnoredFile(rootUriPath, *path, ignoreGlobPatterns))
        return false;

    if (const auto* syntaxError = Luau::get_if<Luau::SyntaxError>(&error.data))
        report(format, humanReadableName.c_str(), error.location, "SyntaxError", syntaxError->message.c_str());
    else
        report(format, humanReadableName.c_str(), error.location, "TypeError",
            Luau::toString(error, Luau::TypeErrorToStringOptions{frontend.fileResolver}).c_str());

    return true;
}

static void reportWarning(ReportFormat format, const char* name, const Luau::LintWarning& warning)
{
    report(format, name, warning.location, Luau::LintWarning::getName(warning.code), warning.text.c_str());
}

static bool analyzeFile(
    Luau::Frontend& frontend, const std::filesystem::path& path, ReportFormat format, bool annotate, std::vector<std::string>& ignoreGlobPatterns)
{
    Luau::CheckResult cr;
    Luau::ModuleName name = path.generic_string();

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
    auto errorFriendlyName = std::filesystem::proximate(path).generic_string();
    std::string humanReadableName = frontend.fileResolver->getHumanReadableModuleName(errorFriendlyName);
    for (auto& error : cr.lintResult.errors)
        reportWarning(format, humanReadableName.c_str(), error);
    for (auto& warning : cr.lintResult.warnings)
        reportWarning(format, humanReadableName.c_str(), warning);

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

int startAnalyze(const argparse::ArgumentParser& program)
{
    ReportFormat format = ReportFormat::Default;
    bool annotate = program.is_used("--annotate");
    auto sourcemapPath = program.present<std::filesystem::path>("--sourcemap");
    auto definitionsPaths = program.get<std::vector<std::filesystem::path>>("--definitions");
    auto ignoreGlobPatterns = program.get<std::vector<std::string>>("--ignore");
    auto baseLuaurc = program.present<std::filesystem::path>("--base-luaurc");
    auto settingsPath = program.present<std::filesystem::path>("--settings");
    std::vector<std::filesystem::path> files{};
    FFlag::DebugLuauTimeTracing.value = program.is_used("--timetrace");

    if (auto filesArg = program.present<std::vector<std::string>>("files"))
    {
        for (const auto& pathString : *filesArg)
        {
            // If the path is not absolute, then we want to construct it into an absolute path
            // by appending it to the current working directory
            auto path = std::filesystem::absolute(pathString);

            if (path != "-" && !std::filesystem::exists(path))
            {
                std::cerr << "Cannot get " << path << ": path does not exist\n";
                return 1;
            }


            if (std::filesystem::is_directory(path))
            {
                for (std::filesystem::recursive_directory_iterator next(path), end; next != end; ++next)
                {
                    if (next->is_regular_file() && next->path().has_extension())
                    {
                        auto ext = next->path().extension();
                        if (ext == ".lua" || ext == ".luau")
                        {
                            files.push_back(next->path());
                        }
                    }
                }
            }
            else
            {
                files.push_back(path);
            }
        }
    }
    else
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

    // Check if files exist
    if (sourcemapPath.has_value() && !std::filesystem::exists(sourcemapPath.value()))
    {
        fprintf(stderr, "Cannot load sourcemap path %s: path does not exist\n", sourcemapPath->generic_string().c_str());
        return 1;
    }

    if (settingsPath.has_value() && !std::filesystem::exists(settingsPath.value()))
    {
        fprintf(stderr, "Cannot load settings path %s: path does not exist\n", settingsPath->generic_string().c_str());
        return 1;
    }

    if (files.empty())
    {
        fprintf(stderr, "error: no files provided\n");
        return 1;
    }

    // Setup Frontend
    Luau::FrontendOptions frontendOptions;
    frontendOptions.retainFullTypeGraphs = annotate;
    frontendOptions.runLintChecks = true;

    CliClient client;

    if (settingsPath)
    {
        if (std::optional<std::string> contents = readFile(*settingsPath))
        {
            client.configuration = dottedToClientConfiguration(contents.value());

            // Process any fflags
            registerFastFlags(client.configuration.fflags.override);
            if (!client.configuration.fflags.enableByDefault)
                std::cerr << "warning: `luau-lsp.fflags.enableByDefault` is not respected in CLI Analyze mode. Please instead use the CLI option "
                             "`--no-flags-enabled` to configure this.\n";
            if (client.configuration.fflags.sync)
                std::cerr << "warning: `luau-lsp.fflags.sync` is not supported in CLI Analyze mode. Instead, all FFlags are enabled by default. "
                             "Please manually configure necessary FFlags\n";
        }
        else
        {
            fprintf(stderr, "Failed to read settings at '%s'\n", settingsPath->generic_string().c_str());
            return 1;
        }
    }

    WorkspaceFileResolver fileResolver;
    if (baseLuaurc)
    {
        Luau::Config result;
        if (std::optional<std::string> contents = readFile(*baseLuaurc))
        {
            std::optional<std::string> error = Luau::parseConfig(*contents, result);
            if (error)
            {
                fprintf(stderr, "%s: %s\n", baseLuaurc->generic_string().c_str(), error->c_str());
                return 1;
            }
            fileResolver = WorkspaceFileResolver(result);
        }
        else
        {
            fprintf(stderr, "Failed to read base .luaurc configuration at '%s'\n", baseLuaurc->generic_string().c_str());
            return 1;
        }
    }

    fileResolver.rootUri = Uri::file(std::filesystem::current_path());
    fileResolver.client = std::make_shared<CliClient>(client);

    if (auto platformArg = program.present("--platform"))
    {
        if (platformArg == "standard")
            client.configuration.platform.type = LSPPlatformConfig::Standard;
        else if (platformArg == "roblox")
            client.configuration.platform.type = LSPPlatformConfig::Roblox;
    }

    std::unique_ptr<LSPPlatform> platform = LSPPlatform::getPlatform(client.configuration, &fileResolver);

    fileResolver.platform = platform.get();

    Luau::Frontend frontend(&fileResolver, &fileResolver, frontendOptions);

    Luau::registerBuiltinGlobals(frontend, frontend.globals, /* typeCheckForAutocomplete = */ false);
    Luau::registerBuiltinGlobals(frontend, frontend.globalsForAutocomplete, /* typeCheckForAutocomplete = */ true);

    for (auto& definitionsPath : definitionsPaths)
    {
        if (!std::filesystem::exists(definitionsPath))
        {
            fprintf(stderr, "Cannot load definitions file %s: path does not exist\n", definitionsPath.generic_string().c_str());
            return 1;
        }

        auto definitionsContents = readFile(definitionsPath);
        if (!definitionsContents)
        {
            fprintf(stderr, "Cannot load definitions file %s: failed to read\n", definitionsPath.generic_string().c_str());
            return 1;
        }

        auto loadResult = types::registerDefinitions(frontend, frontend.globals, *definitionsContents, /* typeCheckForAutocomplete = */ false);
        if (!loadResult.success)
        {
            fprintf(stderr, "Failed to load definitions\n");
            for (const auto& error : loadResult.parseResult.errors)
                report(
                    format, definitionsPath.relative_path().generic_string().c_str(), error.getLocation(), "SyntaxError", error.getMessage().c_str());

            if (loadResult.module)
            {
                for (const auto& error : loadResult.module->errors)
                    if (const auto* syntaxError = Luau::get_if<Luau::SyntaxError>(&error.data))
                        report(format, definitionsPath.relative_path().generic_string().c_str(), error.location, "SyntaxError",
                            syntaxError->message.c_str());
                    else
                        report(format, definitionsPath.relative_path().generic_string().c_str(), error.location, "TypeError",
                            Luau::toString(error).c_str());
            }
            return 1;
        }

        platform->mutateRegisteredDefinitions(frontend.globals, types::parseDefinitionsFileMetadata(*definitionsContents));
    }

    if (sourcemapPath && client.configuration.platform.type == LSPPlatformConfig::Roblox)
    {
        auto robloxPlatform = dynamic_cast<RobloxPlatform*>(platform.get());

        if (auto sourceMapContents = readFile(*sourcemapPath))
        {
            robloxPlatform->updateSourceNodeMap(sourceMapContents.value());

            robloxPlatform->handleSourcemapUpdate(frontend, frontend.globals,
                !program.is_used("--no-strict-dm-types") && client.configuration.diagnostics.strictDatamodelTypes &&
                    client.configuration.platform.roblox.diagnostics.strictDatamodelTypes);
        }
    }

    Luau::freeze(frontend.globals.globalTypes);
    Luau::freeze(frontend.globalsForAutocomplete.globalTypes);

    int failed = 0;

    for (const std::filesystem::path& path : files)
        failed += !analyzeFile(frontend, path, format, annotate, ignoreGlobPatterns);

    if (!client.diagnostics.empty())
    {
        failed += int(client.diagnostics.size());

        for (const auto& [path, err] : client.diagnostics)
            fprintf(stderr, "%s: %s\n", path.generic_string().c_str(), err.c_str());
    }

    if (format == ReportFormat::Luacheck)
        return 0;
    else
        return failed ? 1 : 0;
}
