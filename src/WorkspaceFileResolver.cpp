#include <optional>
#include <unordered_map>
#include <iostream>
#include <regex>
#include <sstream>
#include "Luau/Ast.h"
#include "Luau/LuauConfig.h"
#include "LSP/WorkspaceFileResolver.hpp"
#include "LSP/Workspace.hpp"

#include "Luau/TimeTrace.h"
#include "LuauFileUtils.hpp"
#include "Platform/StringRequireAutoImporter.hpp"

#include "lua.h"

struct LuauConfigInterruptInfo
{
    Luau::TypeCheckLimits limits;
    std::string module;
};

Luau::ModuleName WorkspaceFileResolver::getModuleName(const Uri& name) const
{
    // Handle non-file schemes
    if (name.scheme != "file")
        return name.toString();

    if (auto virtualPath = platform->resolveToVirtualPath(name))
        return *virtualPath;

    return name.fsPath();
}

Uri WorkspaceFileResolver::getUri(const Luau::ModuleName& moduleName) const
{
    if (platform->isVirtualPath(moduleName))
    {
        if (auto uri = platform->resolveToRealPath(moduleName))
            return *uri;
    }

    // TODO: right now we map to file paths for module names, unless it's a non-file uri. Should we store uris directly instead?
    // Then this would be Uri::parse
    return Uri::file(moduleName);
}

const TextDocument* WorkspaceFileResolver::getTextDocument(const lsp::DocumentUri& uri) const
{
    auto it = managedFiles.find(uri);
    if (it != managedFiles.end())
        return &it->second;

    return nullptr;
}

const TextDocument* WorkspaceFileResolver::getTextDocumentFromModuleName(const Luau::ModuleName& name) const
{
    // managedFiles is keyed by URI. If module name is a URI that maps to a managed file, return that directly
    if (auto document = getTextDocument(Uri::parse(name)))
        return document;

    return getTextDocument(getUri(name));
}

TextDocumentPtr WorkspaceFileResolver::getOrCreateTextDocumentFromModuleName(const Luau::ModuleName& name)
{
    if (auto document = getTextDocumentFromModuleName(name))
        return TextDocumentPtr(document);

    if (auto filePath = platform->resolveToRealPath(name))
        if (auto source = readSource(name))
            return TextDocumentPtr(*filePath, "luau", source->source);

    return TextDocumentPtr(nullptr);
}

std::string WorkspaceFileResolver::transformOvertureLoadLibrary(const std::string& source, const Luau::ModuleName& moduleName) const
{
    std::regex singlePattern("local\\s+(\\w+)\\s*=\\s*Overture:LoadLibrary\\s*\\(\\s*\"([^\"]+)\"\\s*\\)");
    std::regex namedPattern("local\\s+([\\w,\\s]+)\\s*=\\s*Overture:LoadLibrary\\s*\\(\\s*\"([^\"]+)\"\\s*,\\s*\\{([^}]+)\\}\\s*\\)");

    std::string result = source;
    std::vector<std::pair<size_t, std::pair<size_t, std::string>>> replacements;

    auto extractFunctionNames = [](const std::string& arrayStr) -> std::vector<std::string>
    {
        std::vector<std::string> names;
        std::regex namePattern("\"([^\"]+)\"");
        std::sregex_iterator iter(arrayStr.begin(), arrayStr.end(), namePattern);
        std::sregex_iterator end;
        for (; iter != end; ++iter)
        {
            names.push_back((*iter)[1].str());
        }
        return names;
    };

    auto splitVarNames = [](const std::string& varList) -> std::vector<std::string>
    {
        std::vector<std::string> names;
        std::stringstream ss(varList);
        std::string name;
        while (std::getline(ss, name, ','))
        {
            // Trim whitespace
            name.erase(0, name.find_first_not_of(" \t\n\r"));
            name.erase(name.find_last_not_of(" \t\n\r") + 1);
            if (!name.empty())
                names.push_back(name);
        }
        return names;
    };

    auto fixRequirePath = [](const std::string& virtualPath) -> std::string
    {
        std::stringstream ss(virtualPath);
        std::string part;
        std::vector<std::string> parts;
        while (std::getline(ss, part, '/'))
            parts.push_back(part);

        auto isValidIdent = [](const std::string& s) -> bool
        {
            if (s.empty()) return false;
            if (!(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) return false;
            for (size_t i = 1; i < s.size(); ++i)
            {
                unsigned char c = static_cast<unsigned char>(s[i]);
                if (!(std::isalnum(c) || c == '_')) return false;
            }
            return true;
        };

        std::string expr;
        for (size_t i = 0; i < parts.size(); ++i)
        {
            const std::string& p = parts[i];
            if (i == 0)
            {
                expr = p;
            }
            else
            {
                if (isValidIdent(p))
                    expr += "." + p;
                else
                    expr += "[\"" + p + "\"]";
            }
        }
        return expr;
    };

    {
        std::string::const_iterator searchStart(source.cbegin());
        std::smatch match;
        while (std::regex_search(searchStart, source.cend(), match, namedPattern))
        {
            std::string varList = match[1].str();
            std::string libName = match[2].str();
            std::string arrayStr = match[3].str();

            size_t matchPos = std::distance(source.cbegin(), match[0].first);
            size_t matchLen = match[0].length();

            std::string replacement = match.str();
            if (workspace)
            {
                if (auto libraryPath = workspace->getOvertureLibraryPath(libName))
                {
                    auto varNames = splitVarNames(varList);
                    auto fnNames = extractFunctionNames(arrayStr);

                    if (varNames.size() == fnNames.size() && !varNames.empty())
                    {
                        const std::string requireExpr = fixRequirePath(*libraryPath);
                        std::string typeAnnotations;

                        for (size_t i = 0; i < varNames.size(); ++i)
                        {
                            if (i > 0)
                                typeAnnotations += ", ";
                            typeAnnotations += varNames[i] + ": typeof(require(" + requireExpr + ")." + fnNames[i] + ")";
                        }

                        replacement = "local " + typeAnnotations + " = Overture:LoadLibrary(\"" + libName + "\", {" + arrayStr + "})";

                        std::cerr << "[Transform] In file: " << moduleName << "\n";
                        std::cerr << "  Original: " << match.str() << "\n";
                        std::cerr << "  Replaced: " << replacement << "\n";
                    }
                }
                else
                {
                    std::cerr << "[Transform] Library path not found for: " << libName << " in " << moduleName << "\n";
                }
            }

            replacements.emplace_back(matchPos, std::make_pair(matchLen, replacement));
            searchStart = match.suffix().first;
        }
    }

    {
        std::string::const_iterator searchStart(source.cbegin());
        std::smatch match;
        while (std::regex_search(searchStart, source.cend(), match, singlePattern))
        {
            std::string varName = match[1].str();
            std::string libName = match[2].str();

            size_t matchPos = std::distance(source.cbegin(), match[0].first);
            size_t matchLen = match[0].length();

            // Skip if already replaced by NamedImports
            bool alreadyReplaced = false;
            for (const auto& r : replacements)
            {
                if (matchPos >= r.first && matchPos < r.first + r.second.first)
                {
                    alreadyReplaced = true;
                    break;
                }
            }

            if (alreadyReplaced)
            {
                searchStart = match.suffix().first;
                continue;
            }

            std::string replacement = match.str();
            if (workspace)
            {
                if (auto libraryPath = workspace->getOvertureLibraryPath(libName))
                {
                    const std::string requireExpr = fixRequirePath(*libraryPath);
                    const std::string typeExpr = "typeof(require(" + requireExpr + "))";
                    replacement = "local " + varName + ": " + typeExpr + " = Overture:LoadLibrary(\"" + libName + "\")";

                    std::cerr << "[Transform] In file: " << moduleName << "\n";
                    std::cerr << "  Original: " << match.str() << "\n";
                    std::cerr << "  Replaced: " << replacement << "\n";
                }
                else
                {
                    std::cerr << "[Transform] Library path not found for: " << libName << " in " << moduleName << "\n";
                }
            }

            replacements.emplace_back(matchPos, std::make_pair(matchLen, replacement));
            searchStart = match.suffix().first;
        }
    }

	// Overture:Get support

    {
        std::regex getPattern(R"(local\s+(\w+)\s*=\s*Overture\s*:\s*Get\s*\(\s*\"([^\"]+)\"\s*,\s*\"([^\"]+)\"(?:\s*,\s*[^)]+)?\s*\))");
        std::string::const_iterator getSearchStart(source.cbegin());
        std::smatch getMatch;
        while (std::regex_search(getSearchStart, source.cend(), getMatch, getPattern))
        {
            std::string varName = getMatch[1].str();
            std::string className = getMatch[2].str();
            std::string instanceName = getMatch[3].str();

            size_t matchPos = std::distance(source.cbegin(), getMatch[0].first);
            size_t matchLen = getMatch[0].length();

            // Check if already replaced
            bool alreadyReplaced = false;
            for (const auto& r : replacements)
            {
                if (matchPos >= r.first && matchPos < r.first + r.second.first)
                {
                    alreadyReplaced = true;
                    break;
                }
            }

            if (!alreadyReplaced)
            {
                std::string originalCall = getMatch.str();
                std::string thirdArgPart;

                size_t secondQuoteEnd = originalCall.rfind('"');
                size_t commaAfterSecond = originalCall.find(',', secondQuoteEnd);

                if (commaAfterSecond != std::string::npos)
                {
                    thirdArgPart = originalCall.substr(commaAfterSecond);
                }

                // Type the variable based on the ClassName
                std::string replacement = "local " + varName + ": " + className + " = Overture:Get(\"" + className + "\", \"" + instanceName + "\"" + thirdArgPart + ")";

                std::cerr << "[Transform] In file: " << moduleName << "\n";
                std::cerr << "  Original: " << originalCall << "\n";
                std::cerr << "  Replaced: " << replacement << "\n";

                replacements.emplace_back(matchPos, std::make_pair(matchLen, replacement));
            }

            getSearchStart = getMatch.suffix().first;
        }
    }

    // Apply replacements in reverse order
    std::sort(replacements.begin(), replacements.end());
    for (auto it = replacements.rbegin(); it != replacements.rend(); ++it)
    {
        result.replace(it->first, it->second.first, it->second.second);
    }

    return result;
}


std::optional<Luau::SourceCode> WorkspaceFileResolver::readSource(const Luau::ModuleName& name)
{
    LUAU_TIMETRACE_SCOPE("WorkspaceFileResolver::readSource", "LSP");
    auto uri = getUri(name);
    auto sourceType = platform->sourceCodeTypeFromPath(uri);

    if (auto source = platform->readSourceCode(name, uri))
    {
        // Only transform Overture:LoadLibrary calls for files that are currently open in the editor
        std::string transformedSource = *source;
        if (managedFiles.find(uri) != managedFiles.end())
        {
            transformedSource = transformOvertureLoadLibrary(*source, name);
        }
        return Luau::SourceCode{transformedSource, sourceType};
    }

    return std::nullopt;
}

std::optional<Luau::ModuleInfo> WorkspaceFileResolver::resolveModule(
    const Luau::ModuleInfo* context, Luau::AstExpr* node, const Luau::TypeCheckLimits& limits)
{
    return platform->resolveModule(context, node, limits);
}

std::string WorkspaceFileResolver::getHumanReadableModuleName(const Luau::ModuleName& name) const
{
    if (platform->isVirtualPath(name))
    {
        if (auto realPath = platform->resolveToRealPath(name))
        {
            return realPath->fsPath() + " [" + name + "]";
        }
        else
        {
            return name;
        }
    }
    else
    {
        return name;
    }
}

const Luau::Config& WorkspaceFileResolver::getConfig(const Luau::ModuleName& name, const Luau::TypeCheckLimits& limits) const
{
    LUAU_TIMETRACE_SCOPE("WorkspaceFileResolver::getConfig", "Frontend");
    auto uri = getUri(name);
    auto base = uri.parent();

    if (base && isInitLuauFile(uri))
        base = base->parent();

    if (!base)
        return defaultConfig;

    return readConfigRec(*base, limits);
}

std::optional<std::string> WorkspaceFileResolver::parseConfig(const Uri& configPath, const std::string& contents, Luau::Config& result, bool compat)
{
    LUAU_ASSERT(configPath.parent());

    Luau::ConfigOptions::AliasOptions aliasOpts;
    aliasOpts.configLocation = configPath.parent()->fsPath();
    aliasOpts.overwriteAliases = true;

    Luau::ConfigOptions opts;
    opts.aliasOptions = std::move(aliasOpts);
    opts.compat = compat;

    return Luau::parseConfig(contents, result, opts);
}

std::optional<std::string> WorkspaceFileResolver::parseLuauConfig(
    const Uri& configPath, const std::string& contents, Luau::Config& result, const Luau::TypeCheckLimits& limits)
{
    LUAU_ASSERT(configPath.parent());

    Luau::ConfigOptions::AliasOptions aliasOpts;
    aliasOpts.configLocation = configPath.parent()->fsPath();
    aliasOpts.overwriteAliases = true;

    Luau::InterruptCallbacks callbacks;
    LuauConfigInterruptInfo info{limits, configPath.fsPath()};
    callbacks.initCallback = [&info](lua_State* L)
    {
        lua_setthreaddata(L, &info);
    };
    callbacks.interruptCallback = [](lua_State* L, int gc)
    {
        auto* info = static_cast<LuauConfigInterruptInfo*>(lua_getthreaddata(L));
        if (info->limits.finishTime && Luau::TimeTrace::getClock() > *info->limits.finishTime)
            throw Luau::TimeLimitError{info->module};
        if (info->limits.cancellationToken && info->limits.cancellationToken->requested())
            throw Luau::UserCancelError{info->module};
    };

    return Luau::extractLuauConfig(contents, result, aliasOpts, std::move(callbacks));
}

const Luau::Config& WorkspaceFileResolver::readConfigRec(const Uri& uri, const Luau::TypeCheckLimits& limits) const
{
    auto it = configCache.find(uri);
    if (it != configCache.end())
        return it->second;

    Luau::Config result = defaultConfig;
    if (const auto& parent = uri.parent())
        result = readConfigRec(*parent, limits);

    auto configPath = uri.resolvePath(Luau::kConfigName);
    auto luauConfigPath = uri.resolvePath(Luau::kLuauConfigName);
    auto robloxRcPath = uri.resolvePath(".robloxrc");

    if (std::optional<std::string> contents = Luau::FileUtils::readFile(luauConfigPath.fsPath()))
    {
        if (client)
            client->sendLogMessage(lsp::MessageType::Info, "Loading Luau configuration from " + luauConfigPath.fsPath());

        std::optional<std::string> error = parseLuauConfig(configPath, *contents, result, limits);
        if (error)
        {
            if (client)
            {
                lsp::Diagnostic diagnostic{{{0, 0}, {0, 0}}};
                diagnostic.message = *error;
                diagnostic.severity = lsp::DiagnosticSeverity::Error;
                diagnostic.source = "Luau";
                client->publishDiagnostics({configPath, std::nullopt, {diagnostic}});
            }
            else
                // TODO: this should never be reached anymore
                std::cerr << configPath.toString() << ": " << *error;
        }
        else
        {
            if (client)
                // Clear errors presented for file
                client->publishDiagnostics({configPath, std::nullopt, {}});
        }
    }
    if (std::optional<std::string> contents = Luau::FileUtils::readFile(configPath.fsPath()))
    {
        if (client)
            client->sendLogMessage(lsp::MessageType::Info, "Loading Luau configuration from " + configPath.fsPath());

        std::optional<std::string> error = parseConfig(configPath, *contents, result);
        if (error)
        {
            if (client)
            {
                lsp::Diagnostic diagnostic{{{0, 0}, {0, 0}}};
                diagnostic.message = *error;
                diagnostic.severity = lsp::DiagnosticSeverity::Error;
                diagnostic.source = "Luau";
                client->publishDiagnostics({configPath, std::nullopt, {diagnostic}});
            }
            else
                // TODO: this should never be reached anymore
                std::cerr << configPath.toString() << ": " << *error;
        }
        else
        {
            if (client)
                // Clear errors presented for file
                client->publishDiagnostics({configPath, std::nullopt, {}});
        }
    }
    else if (std::optional<std::string> robloxRcContents = Luau::FileUtils::readFile(robloxRcPath.fsPath()))
    {
        if (client)
            client->sendLogMessage(lsp::MessageType::Info, "Loading Luau configuration from " + robloxRcPath.fsPath());

        // Backwards compatibility for .robloxrc files
        std::optional<std::string> error = parseConfig(robloxRcPath, *robloxRcContents, result, /* compat = */ true);
        if (error)
        {
            if (client)
            {
                lsp::Diagnostic diagnostic{{{0, 0}, {0, 0}}};
                diagnostic.message = *error;
                diagnostic.severity = lsp::DiagnosticSeverity::Error;
                diagnostic.source = "Luau";
                client->publishDiagnostics({robloxRcPath, std::nullopt, {diagnostic}});
            }
            else
                // TODO: this should never be reached anymore
                std::cerr << robloxRcPath.toString() << ": " << *error;
        }
        else
        {
            if (client)
                // Clear errors presented for file
                client->publishDiagnostics({robloxRcPath, std::nullopt, {}});
        }
    }

    return configCache[uri] = result;
}

void WorkspaceFileResolver::clearConfigCache()
{
    configCache.clear();
}
