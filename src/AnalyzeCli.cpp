// Source: https://github.com/Roblox/luau/blob/master/CLI/Analyze.cpp; licensed under MIT License
#include "Analyze/AnalyzeCli.hpp"
#include "Analyze/CliConfigurationParser.hpp"
#include "Analyze/CliClient.hpp"
#include "Flags.hpp"

#include "LSP/Workspace.hpp"
#include "LSP/ClientConfiguration.hpp"
#include "Luau/TypeAttach.h"
#include "Luau/PrettyPrinter.h"
#include "LuauFileUtils.hpp"
#include "LSP/LuauExt.hpp"
#include "glob/match.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

LUAU_FASTFLAG(DebugLuauTimeTracing)
LUAU_FASTFLAG(LuauSolverV2)


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

static bool reportError(WorkspaceFolder& workspace, ReportFormat format, const Luau::TypeError& error)
{
    auto [uri, relativePath] = getFilePath(&workspace.fileResolver, error.moduleName);

    if (workspace.isIgnoredFile(uri))
        return false;

    if (const auto* syntaxError = Luau::get_if<Luau::SyntaxError>(&error.data))
        report(format, relativePath.c_str(), error.location, "SyntaxError", syntaxError->message.c_str());
    else
        report(format, relativePath.c_str(), error.location, "TypeError",
            Luau::toString(error, Luau::TypeErrorToStringOptions{workspace.frontend.fileResolver}).c_str());

    return true;
}

static void reportWarning(ReportFormat format, const char* name, const Luau::LintWarning& warning)
{
    report(format, name, warning.location, Luau::LintWarning::getName(warning.code), warning.text.c_str());
}

bool analyzeFile(WorkspaceFolder& workspace, const std::string& path, ReportFormat format, bool annotate)
{
    Luau::ModuleName name = workspace.fileResolver.getModuleName(Uri::file(path));

    // Use checkStrict when annotating to retain type graphs needed by attachTypeData
    Luau::CheckResult cr = annotate ? workspace.checkStrict(name, /* cancellationToken= */ nullptr, /* forAutocomplete= */ false)
                                    : workspace.checkSimple(name, /* cancellationToken= */ nullptr);

    if (!workspace.frontend.getSourceModule(name))
    {
        std::cerr << "Error opening " << name << "\n";
        return false;
    }

    unsigned int reportedErrors = 0;
    for (auto& error : cr.errors)
        reportedErrors += reportError(workspace, format, error);

    // For the human readable module name, we use a relative version
    auto [_, relativePath] = getFilePath(&workspace.fileResolver, path);
    for (auto& error : cr.lintResult.errors)
        reportWarning(format, relativePath.c_str(), error);
    for (auto& warning : cr.lintResult.warnings)
        reportWarning(format, relativePath.c_str(), warning);

    if (annotate)
    {
        Luau::SourceModule* sm = workspace.frontend.getSourceModule(name);
        Luau::ModulePtr m = workspace.getModule(name);

        Luau::attachTypeData(*sm, *m);

        std::string annotated = Luau::prettyPrintWithTypes(*sm->root);

        printf("%s", annotated.c_str());
    }

    return reportedErrors == 0 && cr.lintResult.errors.empty();
}

std::vector<std::string> getFilesToAnalyze(const std::vector<std::string>& paths, WorkspaceFolder* workspace)
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
                        if (!workspace || !workspace->isIgnoredFile(uri))
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

void applySettings(const std::string& settingsContents, CliClient& client)
{
    client.globalConfig = dottedToClientConfiguration(settingsContents);

    // Merge definitions from settings into client.definitionsFiles
    for (const auto& pair : client.globalConfig.types.definitionFiles)
        client.definitionsFiles.insert(pair);

    // Process any fflags
    if (client.globalConfig.fflags.enableNewSolver)
        FFlag::LuauSolverV2.value = true;
    registerFastFlagsCLI(client.globalConfig.fflags.override);
    if (!client.globalConfig.fflags.enableByDefault)
        std::cerr << "warning: `luau-lsp.fflags.enableByDefault` is not respected in CLI Analyze mode. Please instead use the CLI option "
                     "`--no-flags-enabled` to configure this.\n";
    if (client.globalConfig.fflags.sync)
        std::cerr << "warning: `luau-lsp.fflags.sync` is not supported in CLI Analyze mode. Instead, all FFlags are enabled by default. "
                     "Please manually configure necessary FFlags\n";
}

// Recursively find all *.d.luau files in a directory
static std::vector<std::string> findDefinitionFilesInDirectory(const std::string& dirPath)
{
    std::vector<std::string> definitionFiles;

    try
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath))
        {
            if (entry.is_regular_file())
            {
                const auto& path = entry.path();
                // Match files ending in .d.luau
                if (path.extension() == ".luau")
                {
                    auto stem = path.stem().string();
                    if (stem.size() > 2 && stem.substr(stem.size() - 2) == ".d")
                    {
                        definitionFiles.push_back(path.string());
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "warning: failed to read directory '" << dirPath << "': " << e.what() << "\n";
    }

    return definitionFiles;
}

// Expand a glob pattern to matching file paths
// Supports: *, ?, [...], ** (recursive directory match)
static std::vector<std::string> expandGlobPattern(const std::string& pattern)
{
    std::vector<std::string> matchingFiles;

    // Find the first glob character
    size_t globPos = pattern.find_first_of("*?[");
    if (globPos == std::string::npos)
    {
        // No glob characters, treat as a literal path
        if (std::filesystem::exists(pattern))
        {
            matchingFiles.push_back(pattern);
        }
        return matchingFiles;
    }

    // Find the last directory separator before the first glob character
    size_t lastSep = pattern.find_last_of("/\\", globPos);

    std::string basePath;
    std::string globPart;

    if (lastSep == std::string::npos)
    {
        // No directory separator before glob, use current directory
        basePath = ".";
        globPart = pattern;
    }
    else
    {
        basePath = pattern.substr(0, lastSep);
        globPart = pattern.substr(lastSep + 1);
    }

    // If base path is empty, use current directory
    if (basePath.empty())
    {
        basePath = ".";
    }
    else
    {
        // Remove trailing separator if present
        while (basePath.size() > 1 && (basePath.back() == '/' || basePath.back() == '\\'))
        {
            basePath.pop_back();
        }
    }

    // Check if base path exists
    if (!std::filesystem::exists(basePath) || !std::filesystem::is_directory(basePath))
    {
        std::cerr << "warning: base directory '" << basePath << "' does not exist or is not a directory\n";
        return matchingFiles;
    }

    // Handle ** pattern
    size_t doubleStarPos = globPart.find("**");
    if (doubleStarPos != std::string::npos)
    {
        // Extract prefix (before **) and suffix (after **)
        std::string prefix = globPart.substr(0, doubleStarPos);
        std::string suffix = globPart.substr(doubleStarPos + 2);

        // Remove trailing separator from prefix
        if (!prefix.empty() && (prefix.back() == '/' || prefix.back() == '\\'))
        {
            prefix.pop_back();
        }

        // Remove leading separator from suffix
        if (!suffix.empty() && (suffix.front() == '/' || suffix.front() == '\\'))
        {
            suffix = suffix.substr(1);
        }

        // Build the full prefix path
        std::string prefixPath = basePath;
        if (!prefix.empty())
        {
            prefixPath += "/" + prefix;
        }

        // If prefix path exists, recursively match
        if (std::filesystem::exists(prefixPath) && std::filesystem::is_directory(prefixPath))
        {
            try
            {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(prefixPath))
                {
                    if (entry.is_regular_file())
                    {
                        auto relativePath = std::filesystem::relative(entry.path(), basePath).string();

                        // For matching, try filename against suffix and full relative path
                        bool matches = false;

                        if (suffix.empty())
                        {
                            // No suffix after **, match all files
                            matches = true;
                        }
                        else
                        {
                            // Try to match the suffix against the filename
                            auto fileName = entry.path().filename().string();
                            matches = glob::glob_match(fileName, suffix);

                            // Also try matching the full relative path
                            if (!matches)
                            {
                                matches = glob::glob_match(relativePath, suffix);
                            }
                        }

                        if (matches)
                        {
                            matchingFiles.push_back(entry.path().string());
                        }
                    }
                }
            }
            catch (const std::exception& e)
            {
                std::cerr << "warning: failed to iterate directory '" << prefixPath << "': " << e.what() << "\n";
            }
        }
    }
    else
    {
        // No ** pattern, use regular recursive matching
        try
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(basePath))
            {
                if (entry.is_regular_file())
                {
                    auto relativePath = std::filesystem::relative(entry.path(), basePath).string();

                    if (glob::glob_match(relativePath, globPart))
                    {
                        matchingFiles.push_back(entry.path().string());
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "warning: failed to iterate directory '" << basePath << "': " << e.what() << "\n";
        }
    }

    return matchingFiles;
}

// Extract package name from file path
static std::string extractPackageNameFromPath(const std::string& filePath)
{
    auto path = std::filesystem::path(filePath);
    auto fileName = path.filename().string();

    // Check if it's a .d.luau file
    if (fileName.size() > 7 && fileName.substr(fileName.size() - 7) == ".d.luau")
    {
        // Remove .d.luau extension
        return "@" + fileName.substr(0, fileName.size() - 7);
    }

    // For other files, use stem
    return "@" + path.stem().string();
}

// Find project root by walking up from CWD looking for .git directory
static std::optional<std::string> findProjectRoot()
{
    auto cwd = Luau::FileUtils::getCurrentWorkingDirectory();
    if (!cwd)
        return std::nullopt;

    std::filesystem::path current(*cwd);
    std::filesystem::path original = current;

    // Walk up looking for .git directory
    while (true)
    {
        std::filesystem::path gitDir = current / ".git";
        if (std::filesystem::exists(gitDir) && std::filesystem::is_directory(gitDir))
            return current.string();

        // If we've reached root, stop
        if (current.parent_path() == current)
            break;

        current = current.parent_path();
    }

    // No .git found, return CWD
    return original.string();
}

// Load config file from JSON
static std::optional<ConfigFileData> parseConfigJson(const std::string& contents, const std::string& configPath)
{
    ConfigFileData config;

    // Get directory of config file for resolving relative paths
    std::filesystem::path configDir = std::filesystem::path(configPath).parent_path();

    try
    {
        auto json = nlohmann::json::parse(contents);
        // NOTE: no exception throwing expected, json::parse handles syntax errors

        // Parse "definitions" array
        if (json.contains("definitions") && json["definitions"].is_array())
        {
            for (const auto& item : json["definitions"])
            {
                if (item.is_string())
                {
                    std::string value = item.get<std::string>();
                    // Resolve @name=path entries: path is relative to config file dir
                    size_t eqIndex = value.find('=');
                    if (eqIndex != std::string::npos)
                    {
                        std::string pathPart = value.substr(eqIndex + 1);
                        if (!Luau::FileUtils::isAbsolutePath(pathPart))
                        {
                            std::filesystem::path resolved = configDir / pathPart;
                            value = value.substr(0, eqIndex + 1) + resolved.string();
                        }
                    }
                    // Also resolve glob patterns and bare file paths
                    else if (value.find_first_of("*?[") != std::string::npos || !Luau::FileUtils::isAbsolutePath(value))
                    {
                        if (!Luau::FileUtils::isAbsolutePath(value))
                        {
                            // Strip VFS @ prefix (e.g. "@A/pub/**/*.d.luau" → "pub/**/*.d.luau")
                            std::string fsPath = value;
                            if (fsPath.size() > 1 && fsPath[0] == '@')
                            {
                                size_t slashPos = fsPath.find('/');
                                if (slashPos != std::string::npos)
                                    fsPath = fsPath.substr(slashPos + 1);
                            }
                            std::filesystem::path resolved = configDir / fsPath;
                            value = resolved.string();
                        }
                    }
                    config.definitions.push_back(value);
                }
            }
        }

        // Parse "definitionsDir" array
        if (json.contains("definitionsDir") && json["definitionsDir"].is_array())
        {
            for (const auto& item : json["definitionsDir"])
            {
                if (item.is_string())
                {
                    std::string dirPath = item.get<std::string>();
                    if (!Luau::FileUtils::isAbsolutePath(dirPath))
                    {
                        std::filesystem::path resolved = configDir / dirPath;
                        dirPath = resolved.string();
                    }
                    config.definitionsDir.push_back(dirPath);
                }
            }
        }

        // Parse "docs" array
        if (json.contains("docs") && json["docs"].is_array())
        {
            for (const auto& item : json["docs"])
            {
                if (item.is_string())
                {
                    std::string docPath = item.get<std::string>();
                    if (!Luau::FileUtils::isAbsolutePath(docPath))
                    {
                        std::filesystem::path resolved = configDir / docPath;
                        docPath = resolved.string();
                    }
                    config.docs.push_back(docPath);
                }
            }
        }

        // Parse "platform" string
        if (json.contains("platform") && json["platform"].is_string())
            config.platform = json["platform"].get<std::string>();

        // Parse "baseLuaurc" string
        if (json.contains("baseLuaurc") && json["baseLuaurc"].is_string())
        {
            std::string luaurcPath = json["baseLuaurc"].get<std::string>();
            if (!Luau::FileUtils::isAbsolutePath(luaurcPath))
            {
                std::filesystem::path resolved = configDir / luaurcPath;
                luaurcPath = resolved.string();
            }
            config.baseLuaurc = luaurcPath;
        }
    }
    catch (const nlohmann::json::parse_error& e)
    {
        std::cerr << "warning: failed to parse config file '" << configPath << "': " << e.what() << "\n";
        return std::nullopt;
    }
    catch (const std::exception& e)
    {
        std::cerr << "warning: error reading config file '" << configPath << "': " << e.what() << "\n";
        return std::nullopt;
    }

    return config;
}

// Merge two ConfigFileData: base is overridden/supplemented by overlay
static ConfigFileData mergeConfigData(const ConfigFileData& base, const ConfigFileData& overlay)
{
    ConfigFileData result = base;

    // Arrays: concatenate, skip exact-duplicate paths
    auto addUnique = [](std::vector<std::string>& target, const std::vector<std::string>& source)
    {
        for (const auto& item : source)
        {
            if (std::find(target.begin(), target.end(), item) == target.end())
                target.push_back(item);
        }
    };

    addUnique(result.definitions, overlay.definitions);
    addUnique(result.definitionsDir, overlay.definitionsDir);
    addUnique(result.docs, overlay.docs);

    // Scalars: overlay wins if non-empty
    if (!overlay.platform.empty())
        result.platform = overlay.platform;
    if (!overlay.baseLuaurc.empty())
        result.baseLuaurc = overlay.baseLuaurc;

    return result;
}

std::optional<ConfigFileData> loadConfigFile(const std::optional<std::string>& configPath)
{
    if (configPath)
    {
        // Explicit path provided, use it directly (no merge)
        auto contents = Luau::FileUtils::readFile(*configPath);
        if (!contents)
        {
            std::cerr << "warning: failed to read config file '" << *configPath << "'\n";
            return std::nullopt;
        }
        return parseConfigJson(*contents, *configPath);
    }

    // Auto-discover: walk from project root down to CWD, collect all configs, merge
    auto projectRoot = findProjectRoot();
    if (!projectRoot)
        return std::nullopt;

    std::filesystem::path root(*projectRoot);
    auto cwd = Luau::FileUtils::getCurrentWorkingDirectory();
    if (!cwd)
        return std::nullopt;

    // Collect directories from CWD up to root, then reverse
    std::vector<std::filesystem::path> dirPath;
    std::filesystem::path cwdPath(*cwd);
    std::filesystem::path current = cwdPath;

    while (true)
    {
        dirPath.push_back(current);
        if (current == root || current.parent_path() == current)
            break;
        current = current.parent_path();
        // Don't go above root
        if (current < root && root.string().find(current.string()) != 0)
            break;
    }
    // Reverse so root is first, CWD is last (closest)
    std::reverse(dirPath.begin(), dirPath.end());

    // Collect and merge configs
    std::optional<ConfigFileData> merged;
    for (const auto& dir : dirPath)
    {
        std::filesystem::path candidate = dir / "luau-lsp-settings.json";
        if (std::filesystem::exists(candidate))
        {
            auto contents = Luau::FileUtils::readFile(candidate.string());
            if (contents)
            {
                auto config = parseConfigJson(*contents, candidate.string());
                if (config)
                {
                    if (merged)
                        merged = mergeConfigData(*merged, *config);
                    else
                        merged = config;
                }
            }
        }
    }

    return merged;
}

std::unordered_map<std::string, std::string> processDefinitionsFilePaths(
    const argparse::ArgumentParser& program, const std::optional<ConfigFileData>& configData)
{
    std::unordered_map<std::string, std::string> definitionsFiles{};

    // First, process config file definitions (if any)
    if (configData)
    {
        for (const auto& definition : configData->definitions)
        {
            size_t eqIndex = definition.find('=');
            if (eqIndex != std::string::npos)
            {
                std::string packageName = definition.substr(0, eqIndex);
                std::string filePath = definition.substr(eqIndex + 1, definition.length());

                if (!Luau::startsWith(packageName, "@"))
                    packageName = "@" + packageName;

                definitionsFiles.emplace(packageName, filePath);
            }
            else if (definition.find_first_of("*?[") != std::string::npos)
            {
                auto matchedFiles = expandGlobPattern(definition);
                for (const auto& filePath : matchedFiles)
                {
                    auto packageName = extractPackageNameFromPath(filePath);
                    if (!definitionsFiles.count(packageName))
                        definitionsFiles.emplace(packageName, filePath);
                }
            }
            else
            {
                auto packageName = extractPackageNameFromPath(definition);
                if (!definitionsFiles.count(packageName))
                    definitionsFiles.emplace(packageName, definition);
            }
        }

        for (const auto& dirPath : configData->definitionsDir)
        {
            auto definitionFiles = findDefinitionFilesInDirectory(dirPath);
            for (const auto& filePath : definitionFiles)
            {
                auto path = std::filesystem::path(filePath);
                auto stem = path.stem().string();
                auto packageName = "@" + stem.substr(0, stem.size() - 2);

                if (!definitionsFiles.count(packageName))
                    definitionsFiles.emplace(packageName, filePath);
            }
        }
    }

    // Then, process CLI definitions (they override config file values)
    for (const auto& definition : program.get<std::vector<std::string>>("--definitions"))
    {
        size_t eqIndex = definition.find('=');
        if (eqIndex != std::string::npos)
        {
            // Explicit @name=path mapping - CLI always wins over config
            std::string packageName = definition.substr(0, eqIndex);
            std::string filePath = definition.substr(eqIndex + 1, definition.length());

            if (!Luau::startsWith(packageName, "@"))
                packageName = "@" + packageName;

            definitionsFiles.insert_or_assign(packageName, filePath);
        }
        else
        {
            // Check if this looks like a glob pattern
            if (definition.find_first_of("*?[") != std::string::npos)
            {
                // Glob pattern - expand and use extractPackageNameFromPath
                auto matchedFiles = expandGlobPattern(definition);
                for (const auto& filePath : matchedFiles)
                {
                    auto packageName = extractPackageNameFromPath(filePath);
                    if (!definitionsFiles.count(packageName))
                    {
                        definitionsFiles.emplace(packageName, filePath);
                    }
                }
            }
            else
            {
                // Bare file path - use extractPackageNameFromPath directly
                auto packageName = extractPackageNameFromPath(definition);
                if (!definitionsFiles.count(packageName))
                {
                    definitionsFiles.emplace(packageName, definition);
                }
            }
        }
    }

    // Process --definitions-dir flags
    for (const auto& dirPath : program.get<std::vector<std::string>>("--definitions-dir"))
    {
        auto definitionFiles = findDefinitionFilesInDirectory(dirPath);
        for (const auto& filePath : definitionFiles)
        {
            // Extract package name from filename: foo.d.luau -> @foo
            auto path = std::filesystem::path(filePath);
            auto stem = path.stem().string();  // "foo.d"
            auto packageName = "@" + stem.substr(0, stem.size() - 2);  // remove ".d"

            // Only add if not already defined via --definitions
            if (!definitionsFiles.count(packageName))
            {
                definitionsFiles.emplace(packageName, filePath);
            }
        }
    }

    return definitionsFiles;
}

int startAnalyze(const argparse::ArgumentParser& program)
{
    ReportFormat format = ReportFormat::Default;
    bool annotate = program.is_used("--annotate");
    auto sourcemapPath = program.present<std::string>("--sourcemap");
    auto baseLuaurc = program.present<std::string>("--base-luaurc");
    auto settingsPath = program.present<std::string>("--settings");
    FFlag::DebugLuauTimeTracing.value = program.is_used("--timetrace");

    CliClient client;

    auto currentWorkingDirectory = Luau::FileUtils::getCurrentWorkingDirectory();
    if (!currentWorkingDirectory)
    {
        fprintf(stderr, "Failed to determine current working directory\n");
        return 1;
    }

    // Load config file (if any)
    auto configData = loadConfigFile(program.present<std::string>("--config"));

    // Apply config file values for platform if CLI not specified
    if (configData && !configData->platform.empty() && !program.present("--platform"))
    {
        if (configData->platform == "standard")
            client.globalConfig.platform.type = LSPPlatformConfig::Standard;
        else if (configData->platform == "roblox")
            client.globalConfig.platform.type = LSPPlatformConfig::Roblox;
    }

    // Apply config file values for baseLuaurc if CLI not specified
    if (!baseLuaurc && configData && !configData->baseLuaurc.empty())
        baseLuaurc = configData->baseLuaurc;

    if (settingsPath)
    {
        if (std::optional<std::string> contents = Luau::FileUtils::readFile(*settingsPath))
        {
            applySettings(contents.value(), client);
        }
        else
        {
            fprintf(stderr, "Failed to read settings at '%s'\n", settingsPath->c_str());
            return 1;
        }
    }

    // Apply CLI args after settings so they take precedence
    auto cliIgnoreGlobs = program.get<std::vector<std::string>>("--ignore");
    client.globalConfig.ignoreGlobs.insert(client.globalConfig.ignoreGlobs.end(), cliIgnoreGlobs.begin(), cliIgnoreGlobs.end());
    for (const auto& [key, value] : processDefinitionsFilePaths(program, configData))
        client.definitionsFiles.insert_or_assign(key, value);

    auto filesArg = program.present<std::vector<std::string>>("files");
    if (!filesArg || filesArg->empty())
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

    if (auto platformArg = program.present("--platform"))
    {
        if (platformArg == "standard")
            client.globalConfig.platform.type = LSPPlatformConfig::Standard;
        else if (platformArg == "roblox")
            client.globalConfig.platform.type = LSPPlatformConfig::Roblox;
    }

    if (client.globalConfig.platform.type == LSPPlatformConfig::Roblox && client.definitionsFiles.empty())
    {
        fprintf(stderr, "WARNING: --platform is set to 'roblox' but no definitions files are provided. 'luau-lsp analyze' does not download "
                        "definitions files; use `--platform=standard` to silence\n");
    }

    // Configure sourcemap via configuration (handled by RobloxPlatform::setupWithConfiguration)
    if (sourcemapPath)
    {
        if (client.globalConfig.platform.type == LSPPlatformConfig::Roblox)
        {
            client.globalConfig.sourcemap.sourcemapFile = *sourcemapPath;
            client.globalConfig.sourcemap.enabled = true;
        }
        else
        {
            std::cerr << "warning: a sourcemap was provided, but the current platform is not `roblox`. Use `--platform roblox` to ensure the "
                         "sourcemap option is respected.\n";
        }
    }
    else
    {
        client.globalConfig.sourcemap.enabled = false;
    }

    // Handle deprecated --no-strict-dm-types flag via configuration
    if (program.is_used("--no-strict-dm-types"))
        client.globalConfig.diagnostics.strictDatamodelTypes = false;

    client.globalConfig.index.enabled = false;

    std::optional<Luau::Config> defaultConfig;
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
            defaultConfig = std::move(result);
        }
        else
        {
            fprintf(stderr, "Failed to read base .luaurc configuration at '%s'\n", baseLuaurc->c_str());
            return 1;
        }
    }

    auto rootUri = Uri::file(*currentWorkingDirectory);
    WorkspaceFolder workspace(&client, "CLI", rootUri, defaultConfig);
    workspace.setupWithConfiguration(client.globalConfig);
    workspace.isReady = true;

    auto files = getFilesToAnalyze(*filesArg, &workspace);

    if (files.empty())
    {
        fprintf(stderr, "error: no files provided\n");
        return 1;
    }

    int failed = 0;

    for (const auto& path : files)
        failed += !analyzeFile(workspace, path, format, annotate);

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
