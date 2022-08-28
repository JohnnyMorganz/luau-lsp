// Source: https://github.com/Roblox/luau/blob/master/CLI/Analyze.cpp; licensed under MIT License
#include "Luau/ModuleResolver.h"
#include "Luau/TypeInfer.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/Frontend.h"
#include "Luau/TypeAttach.h"
#include "Luau/Transpiler.h"
#include "LSP/LuauExt.hpp"
#include "LSP/WorkspaceFileResolver.hpp"
#include "LSP/Utils.hpp"
#include "Analyze/AnalyzeCli.hpp"
#include <iostream>
#include <filesystem>
#include <vector>

LUAU_FASTFLAG(DebugLuauTimeTracing)
LUAU_FASTFLAG(LuauTypeMismatchModuleNameResolution)

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
        int columnEnd = (loc.begin.line == loc.end.line) ? loc.end.column : 100;

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

static void reportError(const Luau::Frontend& frontend, ReportFormat format, const Luau::TypeError& error)
{
    std::string humanReadableName = frontend.fileResolver->getHumanReadableModuleName(error.moduleName);

    if (const Luau::SyntaxError* syntaxError = Luau::get_if<Luau::SyntaxError>(&error.data))
        report(format, humanReadableName.c_str(), error.location, "SyntaxError", syntaxError->message.c_str());
    else if (FFlag::LuauTypeMismatchModuleNameResolution)
        report(format, humanReadableName.c_str(), error.location, "TypeError",
            Luau::toString(error, Luau::TypeErrorToStringOptions{frontend.fileResolver}).c_str());
    report(format, humanReadableName.c_str(), error.location, "TypeError", Luau::toString(error).c_str());
}

static void reportWarning(ReportFormat format, const char* name, const Luau::LintWarning& warning)
{
    report(format, name, warning.location, Luau::LintWarning::getName(warning.code), warning.text.c_str());
}

static bool analyzeFile(Luau::Frontend& frontend, const char* name, ReportFormat format, bool annotate)
{
    Luau::CheckResult cr;

    if (frontend.isDirty(name))
        cr = frontend.check(name);

    if (!frontend.getSourceModule(name))
    {
        fprintf(stderr, "Error opening %s\n", name);
        return false;
    }

    for (auto& error : cr.errors)
        reportError(frontend, format, error);

    Luau::LintResult lr = frontend.lint(name);

    std::string humanReadableName = frontend.fileResolver->getHumanReadableModuleName(name);
    for (auto& error : lr.errors)
        reportWarning(format, humanReadableName.c_str(), error);
    for (auto& warning : lr.warnings)
        reportWarning(format, humanReadableName.c_str(), warning);

    if (annotate)
    {
        Luau::SourceModule* sm = frontend.getSourceModule(name);
        Luau::ModulePtr m = frontend.moduleResolver.getModule(name);

        Luau::attachTypeData(*sm, *m);

        std::string annotated = Luau::transpileWithTypes(*sm->root);

        printf("%s", annotated.c_str());
    }

    return cr.errors.empty() && lr.errors.empty();
}

int startAnalyze(int argc, char** argv)
{
    ReportFormat format = ReportFormat::Default;
    bool annotate = false;
    std::optional<std::filesystem::path> sourcemapPath = std::nullopt;
    std::vector<std::filesystem::path> definitionsPaths;
    std::vector<std::filesystem::path> files;

    for (int i = 2; i < argc; ++i)
    {
        if (argv[i][0] == '-')
        {
            // Handle options
            if (strcmp(argv[i], "--formatter=plain") == 0)
                format = ReportFormat::Luacheck;
            else if (strcmp(argv[i], "--formatter=gnu") == 0)
                format = ReportFormat::Gnu;
            else if (strcmp(argv[i], "--annotate") == 0)
                annotate = true;
            else if (strcmp(argv[i], "--timetrace") == 0)
                FFlag::DebugLuauTimeTracing.value = true;
            else if (strncmp(argv[i], "--sourcemap=", 12) == 0)
                sourcemapPath = std::string(argv[i] + 12);
            else if (strncmp(argv[i], "--definitions=", 14) == 0)
                definitionsPaths.push_back(std::string(argv[i] + 14));
            // Backwards compatibility
            else if (strncmp(argv[i], "--defs=", 7) == 0)
                definitionsPaths.push_back(std::string(argv[i] + 7));
        }
        else
        {
            auto path = std::filesystem::path(argv[i]);
            if (path != "-" && !std::filesystem::exists(path))
            {
                std::cerr << "Cannot get " << path << ": path does not exist\n";
                continue;
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

    if (files.empty())
    {
        fprintf(stderr, "error: no files provided\n");
        return 1;
    }

    // Setup Frontend
    Luau::FrontendOptions frontendOptions;
    frontendOptions.retainFullTypeGraphs = annotate;

    WorkspaceFileResolver fileResolver(Uri::file(std::filesystem::current_path()));
    Luau::Frontend frontend(&fileResolver, &fileResolver, frontendOptions);

    if (sourcemapPath)
    {
        if (auto sourceMapContents = readFile(*sourcemapPath))
        {
            fileResolver.updateSourceMap(sourceMapContents.value());
            types::registerInstanceTypes(frontend.typeChecker, fileResolver);
        }
    }

    Luau::registerBuiltinTypes(frontend.typeChecker);
    Luau::registerBuiltinTypes(frontend.typeCheckerForAutocomplete);

    for (auto& definitionsPath : definitionsPaths)
    {
        if (!std::filesystem::exists(definitionsPath))
        {
            fprintf(stderr, "Cannot load definitions file %s: path does not exist\n", definitionsPath.generic_string().c_str());
            return 1;
        }

        auto loadResult = types::registerDefinitions(frontend.typeChecker, definitionsPath);
        if (!loadResult.success)
        {
            fprintf(stderr, "Failed to load definitions\n");
            for (const auto& error : loadResult.parseResult.errors)
                report(
                    format, definitionsPath.relative_path().generic_string().c_str(), error.getLocation(), "SyntaxError", error.getMessage().c_str());

            if (loadResult.module)
            {
                for (const auto& error : loadResult.module->errors)
                    if (const Luau::SyntaxError* syntaxError = Luau::get_if<Luau::SyntaxError>(&error.data))
                        report(format, definitionsPath.relative_path().generic_string().c_str(), error.location, "SyntaxError",
                            syntaxError->message.c_str());
                    else
                        report(format, definitionsPath.relative_path().generic_string().c_str(), error.location, "TypeError",
                            Luau::toString(error).c_str());
            }
        }
    }

    types::registerInstanceTypes(frontend.typeChecker, fileResolver);

    Luau::freeze(frontend.typeChecker.globalTypes);

    int failed = 0;

    for (const std::filesystem::path& path : files)
        failed += !analyzeFile(frontend, path.relative_path().generic_string().c_str(), format, annotate);

    if (!fileResolver.configErrors.empty())
    {
        failed += int(fileResolver.configErrors.size());

        for (const auto& [path, err] : fileResolver.configErrors)
            fprintf(stderr, "%s: %s\n", path.generic_string().c_str(), err.c_str());
    }

    if (format == ReportFormat::Luacheck)
        return 0;
    else
        return failed ? 1 : 0;
}