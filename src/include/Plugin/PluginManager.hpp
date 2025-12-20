#pragma once
#include "Plugin/TextEdit.hpp"
#include "Plugin/SourceMapping.hpp"
#include "Plugin/PluginRuntime.hpp"
#include "LSP/Uri.hpp"
#include <memory>
#include <string>
#include <vector>

struct BaseClient;

namespace lsp
{
enum struct MessageType;
}

namespace Luau::LanguageServer::Plugin
{

// Manages multiple plugins and handles plugin chaining
class PluginManager
{
    std::vector<std::unique_ptr<PluginRuntime>> plugins;
    BaseClient* client = nullptr;

public:
    PluginManager() = default;

    explicit PluginManager(BaseClient* client)
        : client(client)
    {
    }

    // Configure plugins from paths. Returns number of successfully loaded plugins.
    size_t configure(const std::vector<std::string>& pluginPaths, size_t timeoutMs = 5000);

    // Apply all plugins to transform source code.
    // Returns edits to apply, or empty vector if no transformation or error.
    std::vector<TextEdit> transform(const std::string& source, const Uri& uri, const std::string& moduleName);

    // Create a SourceMapping from the given source and edits
    SourceMapping createMapping(const std::string& source, const std::vector<TextEdit>& edits);

    bool hasPlugins() const
    {
        return !plugins.empty();
    }

    size_t pluginCount() const
    {
        return plugins.size();
    }

    // Clear all plugins
    void clear()
    {
        plugins.clear();
    }

private:
    void sendLogMessage(lsp::MessageType type, const std::string& message) const;
};

} // namespace Luau::LanguageServer::Plugin
