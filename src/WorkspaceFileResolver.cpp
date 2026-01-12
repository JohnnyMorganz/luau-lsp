#include <optional>
#include <unordered_map>
#include <iostream>
#include "Luau/Ast.h"
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
    auto edits = pluginManager->transform(original->getText(), uri, moduleName);

    if (edits.empty())
        return original;  // No transformation or error

    // Build plugin document with mapping
    try
    {
        auto mapping = Luau::LanguageServer::Plugin::SourceMapping::fromEdits(original->getText(), edits);
        // Extract transformed source before moving mapping - C++ argument evaluation order is unspecified,
        // so std::move(mapping) could be evaluated before mapping.getTransformedSource()
        auto transformedSource = mapping.getTransformedSource();
        auto pluginDoc = std::make_unique<Luau::LanguageServer::Plugin::PluginTextDocument>(
            original->uri(),
            original->languageId(),
            original->version(),
            original->getText(),
            std::move(transformedSource),
            std::move(mapping));

        pluginDocuments[uri] = std::move(pluginDoc);
        return pluginDocuments[uri].get();
    }
    catch (const std::exception& e)
    {
        if (client)
            client->sendLogMessage(lsp::MessageType::Error, "Failed to create plugin document: " + std::string(e.what()));
        return original;
    }
}

void WorkspaceFileResolver::invalidatePluginDocument(const lsp::DocumentUri& uri)
{
    pluginDocuments.erase(uri);
}

void WorkspaceFileResolver::clearPluginDocuments()
{
    pluginDocuments.clear();
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
        // Apply plugins to non-managed files too
        if (pluginManager && pluginManager->hasPlugins())
        {
            auto edits = pluginManager->transform(*source, uri, name);
            if (!edits.empty())
            {
                try
                {
                    auto mapping = Luau::LanguageServer::Plugin::SourceMapping::fromEdits(*source, edits);
                    return Luau::SourceCode{mapping.getTransformedSource(), sourceType};
                }
                catch (const std::exception& e)
                {
                    if (client)
                        client->sendLogMessage(lsp::MessageType::Error, "Failed to apply plugin transformation: " + std::string(e.what()));
                }
            }
        }
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
