#include <algorithm>
#include <optional>
#include <unordered_map>
#include <iostream>
#include "Luau/Ast.h"
#include "Luau/Lexer.h"
#include "Luau/LuauConfig.h"
#include "LSP/WorkspaceFileResolver.hpp"

#include "Luau/TimeTrace.h"
#include "LuauFileUtils.hpp"

#include "Plugin/PluginManager.hpp"
#include "Plugin/PluginTextDocument.hpp"
#include "Plugin/SourceMapping.hpp"

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

const TextDocument* WorkspaceFileResolver::getManagedTextDocument(const lsp::DocumentUri& uri) const
{
    auto it = managedFiles.find(uri);
    if (it != managedFiles.end())
        return &it->second;

    return nullptr;
}

const TextDocument* WorkspaceFileResolver::getTextDocument(const lsp::DocumentUri& uri) const
{
    // Get the original managed document
    auto* original = getManagedTextDocument(uri);
    if (!original)
        return nullptr;

    // If no plugins active, return original
    if (!pluginManager || !pluginManager->hasPlugins())
        return original;

    // Check cache for existing plugin document
    auto it = pluginDocuments.find(uri);
    if (it != pluginDocuments.end() && it->second && it->second->version() == original->version())
        return it->second.get();

    // Apply plugins to transform the document
    auto moduleName = getModuleName(uri);
    auto transformed = applyPluginTransformation(original->getText(), uri, moduleName);

    if (!transformed)
        return original;

    auto pluginDoc = std::make_unique<Luau::LanguageServer::Plugin::PluginTextDocument>(
        original->uri(),
        original->languageId(),
        original->version(),
        original->getText(),
        std::move(transformed->transformedSource),
        Luau::LanguageServer::Plugin::SourceMapping{std::move(transformed->edits)});

    pluginDocuments[uri] = std::move(pluginDoc);
    return pluginDocuments[uri].get();
}

void WorkspaceFileResolver::invalidatePluginDocument(const lsp::DocumentUri& uri)
{
    pluginDocuments.erase(uri);
}

void WorkspaceFileResolver::clearPluginDocuments()
{
    pluginDocuments.clear();
}

std::optional<Luau::LanguageServer::Plugin::TransformResult> WorkspaceFileResolver::applyPluginTransformation(
    const std::string& source, const Uri& uri, const std::string& moduleName) const
{
    if (!pluginManager || !pluginManager->hasPlugins())
        return std::nullopt;

    // Plugins should not apply to their own source files
    if (pluginManager->isPluginFile(uri))
        return std::nullopt;

    auto edits = pluginManager->transform(source, uri, moduleName);
    if (edits.empty())
        return std::nullopt;

    try
    {
        return Luau::LanguageServer::Plugin::SourceMapping::fromEdits(source, edits);
    }
    catch (const std::exception& e)
    {
        if (client)
            client->sendLogMessage(lsp::MessageType::Error, "Failed to apply plugin transformation: " + std::string(e.what()));
        return std::nullopt;
    }
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

std::optional<Luau::SourceCode> WorkspaceFileResolver::readSource(const Luau::ModuleName& name)
{
    LUAU_TIMETRACE_SCOPE("WorkspaceFileResolver::readSource", "LSP");
    auto uri = getUri(name);
    auto sourceType = platform->sourceCodeTypeFromPath(uri);

    // Check if this is a managed file - use getTextDocument which handles plugin transformation
    if (auto* textDoc = getTextDocument(uri))
        return Luau::SourceCode{textDoc->getText(), sourceType};

    // Fallback to reading from platform
    if (auto source = platform->readSourceCode(name, uri))
    {
        if (auto transformed = applyPluginTransformation(*source, uri, name))
            return Luau::SourceCode{std::move(transformed->transformedSource), sourceType};
        return Luau::SourceCode{*source, sourceType};
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

static std::optional<std::string> filterUnknownLints(const Uri& configPath, const std::string& contents, const Client* client)
{
    using Luau::Lexeme;

    Luau::Allocator allocator;
    Luau::AstNameTable names(allocator);
    Luau::Lexer lexer(contents.data(), contents.size(), names);
    auto next = [&]
    {
        lexer.next();
        while (lexer.current().type == Lexeme::FloorDiv)
            lexer.nextline();
        return lexer.current().type;
    };
    auto lintEntryEnd = [&]() -> std::optional<size_t>
    {
        if (next() != ':')
            return std::nullopt;

        auto value = next();
        if (value != Lexeme::QuotedString && value != Lexeme::ReservedTrue && value != Lexeme::ReservedFalse)
            return std::nullopt;

        auto separator = next();
        if (separator == ',')
            return lexer.getOffset();
        if (separator == '}')
            return lexer.getOffset() - 1;
        return std::nullopt;
    };

    std::optional<std::string> filtered;
    int depth = 0;
    bool inLint = false;
    auto previous = Lexeme::Eof;
    for (next(); lexer.current().type != Lexeme::Eof; next())
    {
        const auto& token = lexer.current();
        if ((previous == '{' || previous == ',') && token.type == Lexeme::QuotedString)
        {
            std::string_view key(token.data, token.getLength());
            if (depth == 1 && key == "lint")
                inLint = next() == ':' && next() == '{';
            else if (depth == 2 && inLint && key != "*" && Luau::LintWarning::parseName(std::string(key).c_str()) == Luau::LintWarning::Code_Unknown)
            {
                if (auto end = lintEntryEnd())
                {
                    if (!filtered)
                        filtered = contents;
                    const auto begin = key.data() - contents.data() - 1;
                    std::replace_if(filtered->begin() + begin, filtered->begin() + *end, [](char ch) { return ch != '\n' && ch != '\r'; }, ' ');

                    std::string warning = configPath.fsPath() + ": Ignoring unknown lint " + std::string(key);
                    if (client)
                        client->sendLogMessage(lsp::MessageType::Warning, warning);
                    else
                        std::cerr << "Warning: " << warning << '\n';
                }
            }
        }

        previous = lexer.current().type;
        if (previous == '{' || previous == '[')
            ++depth;
        else if (previous == '}' || previous == ']')
        {
            --depth;
            if (depth == 1)
                inLint = false;
        }
    }
    return filtered;
}

std::optional<std::string> WorkspaceFileResolver::parseConfig(
    const Uri& configPath, const std::string& contents, Luau::Config& result, const Client* client, bool compat)
{
    LUAU_ASSERT(configPath.parent());

    Luau::ConfigOptions::AliasOptions aliasOpts;
    aliasOpts.configLocation = configPath.parent()->fsPath();
    aliasOpts.overwriteAliases = true;

    Luau::ConfigOptions opts;
    opts.aliasOptions = std::move(aliasOpts);
    opts.compat = compat;

    auto filtered = compat ? std::nullopt : filterUnknownLints(configPath, contents, client);
    return Luau::parseConfig(filtered ? *filtered : contents, result, opts);
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

        std::optional<std::string> error = parseConfig(configPath, *contents, result, client);
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
        std::optional<std::string> error = parseConfig(robloxRcPath, *robloxRcContents, result, client, /* compat = */ true);
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

bool WorkspaceFileResolver::isPluginFile(const Luau::ModuleName& name) const
{
    if (!pluginManager || !pluginManager->hasPlugins())
        return false;

    auto uri = getUri(name);
    if (uri.scheme != "file")
        return false;

    return pluginManager->isPluginFile(uri);
}

std::optional<std::string> WorkspaceFileResolver::getEnvironmentForModule(const Luau::ModuleName& name) const
{
    if (isPluginFile(name))
        return "LSPPlugin";
    return std::nullopt;
}
