#pragma once
#include "Plugin/TextEdit.hpp"
#include "Luau/NotNull.h"
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

struct lua_State;
class WorkspaceFolder;

namespace Luau::LanguageServer::Plugin
{

// Executes Luau plugin scripts in a sandboxed environment
class PluginRuntime
{
public:
    static constexpr int LUA_NOREF = -1;

private:
    std::unique_ptr<lua_State, void (*)(lua_State*)> state;
    std::string pluginPath;
    size_t timeoutMs;
    bool loaded = false;
    int transformSourceRef = LUA_NOREF;
    Luau::NotNull<WorkspaceFolder> workspace;

    // Timeout tracking using wall-clock time
    mutable double deadline = 0;
    mutable bool timedOut = false;

    static void interruptCallback(lua_State* L, int gc);

public:

    explicit PluginRuntime(Luau::NotNull<WorkspaceFolder> workspace, const std::string& pluginPath, size_t timeoutMs = 5000);
    ~PluginRuntime();

    // Non-copyable
    PluginRuntime(const PluginRuntime&) = delete;
    PluginRuntime& operator=(const PluginRuntime&) = delete;

    // Load the plugin script. Returns error if loading fails.
    std::optional<PluginError> load();

    // Transform source code using the plugin
    // Returns list of edits on success, or error on failure
    std::variant<std::vector<TextEdit>, PluginError> transformSource(const std::string& source, const PluginContext& context);

    bool isLoaded() const
    {
        return loaded;
    }

    bool hasTransformSource() const
    {
        return transformSourceRef != LUA_NOREF;
    }

    const std::string& getPath() const
    {
        return pluginPath;
    }
};

} // namespace Luau::LanguageServer::Plugin
