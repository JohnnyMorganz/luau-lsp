#include "Plugin/PluginManager.hpp"
#include "LSP/Client.hpp"
#include "Protocol/Window.hpp"

namespace Luau::LanguageServer::Plugin
{

size_t PluginManager::configure(const std::vector<std::string>& pluginPaths, size_t timeoutMs)
{
    plugins.clear();
    size_t loaded = 0;

    for (const auto& path : pluginPaths)
    {
        sendLogMessage(lsp::MessageType::Info, "Loading plugin: " + path);

        auto plugin = std::make_unique<PluginRuntime>(path, timeoutMs);

        if (auto error = plugin->load())
        {
            sendLogMessage(lsp::MessageType::Error, "Failed to load plugin '" + path + "': " + error->message);
            continue;
        }

        sendLogMessage(lsp::MessageType::Info, "Successfully loaded plugin: " + path);
        plugins.push_back(std::move(plugin));
        loaded++;
    }

    return loaded;
}

std::vector<TextEdit> PluginManager::transform(const std::string& source, const Uri& uri, const std::string& moduleName)
{
    if (plugins.empty())
        return {};

    std::vector<TextEdit> allEdits;
    PluginContext context{uri.fsPath(), moduleName, "luau"};

    // All plugins receive the original source and return edits against it
    for (auto& plugin : plugins)
    {
        auto result = plugin->transformSource(source, context);

        if (auto* error = std::get_if<PluginError>(&result))
        {
            sendLogMessage(lsp::MessageType::Error, "Plugin '" + plugin->getPath() + "' error: " + error->message);
            continue;
        }

        auto& edits = std::get<std::vector<TextEdit>>(result);
        for (auto& edit : edits)
        {
            allEdits.push_back(std::move(edit));
        }
    }

    if (allEdits.empty())
        return {};

    // Validate combined edits - SourceMapping::fromEdits will throw if edits overlap
    try
    {
        SourceMapping::fromEdits(source, allEdits);
    }
    catch (const std::exception& e)
    {
        sendLogMessage(lsp::MessageType::Error, "Plugin edits overlap: " + std::string(e.what()));
        return {};
    }

    return allEdits;
}

SourceMapping PluginManager::createMapping(const std::string& source, const std::vector<TextEdit>& edits)
{
    if (edits.empty())
        return SourceMapping{};

    return SourceMapping::fromEdits(source, edits);
}

void PluginManager::sendLogMessage(lsp::MessageType type, const std::string& message) const
{
    if (client)
        client->sendLogMessage(type, message);
}

} // namespace Luau::LanguageServer::Plugin
