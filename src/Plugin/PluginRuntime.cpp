#include "Plugin/PluginRuntime.hpp"
#include "Plugin/PluginLuaApi.hpp"
#include "LuauFileUtils.hpp"
#include "Luau/Compiler.h"
#include "Luau/TimeTrace.h"

#include "lua.h"
#include "lualib.h"

namespace Luau::LanguageServer::Plugin
{

void PluginRuntime::interruptCallback(lua_State* L, int gc)
{
    if (gc >= 0)
        return; // GC interrupt, not execution interrupt

    // Get the PluginRuntime instance from thread data
    auto* runtime = static_cast<PluginRuntime*>(lua_getthreaddata(L));
    if (!runtime)
        return;

    // Check if we've exceeded the timeout using wall-clock time
    double currentTime = Luau::TimeTrace::getClock();
    if (runtime->deadline > 0 && currentTime > runtime->deadline)
    {
        runtime->timedOut = true;
        lua_break(L);
    }
}

PluginRuntime::PluginRuntime(Luau::NotNull<WorkspaceFolder> workspace, const Uri& pluginUri, size_t timeoutMs)
    : state(nullptr, lua_close)
    , pluginUri(pluginUri)
    , timeoutMs(timeoutMs)
    , workspace(workspace)
{
}

PluginRuntime::~PluginRuntime()
{
    // Clean up the reference if we have one
    if (state && transformSourceRef != LUA_NOREF)
    {
        lua_unref(state.get(), transformSourceRef);
    }
}

std::optional<PluginError> PluginRuntime::load()
{
    auto pluginPath = pluginUri.fsPath();

    // Read plugin file
    auto source = Luau::FileUtils::readFile(pluginPath);
    if (!source)
    {
        return PluginError{"Failed to open plugin file: " + pluginPath, pluginPath};
    }

    // Create Lua state
    state.reset(luaL_newstate());
    lua_State* L = state.get();

    // Open safe libraries
    luaL_openlibs(L);

    // Register Uri userdata metatable (must be before sandbox)
    registerUriUserdata(L);

    // Register lsp API and print override (must be before sandbox)
    registerLspApi(L, workspace, pluginPath);

    // Sandbox the environment (removes dangerous functions)
    luaL_sandbox(L);

    // Store pointer to this runtime for interrupt callback
    lua_setthreaddata(L, this);

    // Set up interrupt callback for timeout enforcement
    lua_callbacks(L)->interrupt = interruptCallback;

    // Compile the plugin
    Luau::CompileOptions compileOptions;
    compileOptions.debugLevel = 1;
    compileOptions.optimizationLevel = 1;

    std::string bytecode = Luau::compile(*source, compileOptions);
    if (bytecode.empty())
    {
        return PluginError{"Failed to compile plugin", pluginPath};
    }

    // Load the bytecode
    std::string chunkName = "=" + pluginPath;
    if (luau_load(L, chunkName.c_str(), bytecode.data(), bytecode.size(), 0) != 0)
    {
        std::string error = lua_tostring(L, -1);
        lua_pop(L, 1);
        return PluginError{"Failed to load plugin: " + error, pluginPath};
    }

    // Execute the plugin to get the return value (should be a table)
    // Set deadline for timeout
    deadline = Luau::TimeTrace::getClock() + (timeoutMs / 1000.0);
    timedOut = false;

    int status = lua_resume(L, nullptr, 0);
    if (status != LUA_OK)
    {
        if (timedOut)
        {
            return PluginError{"Plugin execution timed out", pluginPath};
        }

        std::string error = lua_isstring(L, -1) ? lua_tostring(L, -1) : "Unknown error";
        lua_pop(L, 1);
        return PluginError{"Plugin execution failed: " + error, pluginPath};
    }

    // Check that the plugin returned a table
    if (lua_gettop(L) != 1 || !lua_istable(L, -1))
    {
        return PluginError{"Plugin must return a table", pluginPath};
    }

    // Get the transformSource function (optional)
    lua_getfield(L, -1, "transformSource");
    if (lua_isfunction(L, -1))
    {
        // Store reference to the function
        transformSourceRef = lua_ref(L, -1);
    }
    else if (!lua_isnil(L, -1))
    {
        // transformSource exists but is not a function - that's an error
        lua_pop(L, 2); // Pop field and table
        return PluginError{"Plugin 'transformSource' must be a function", pluginPath};
    }
    else
    {
        // transformSource not provided - that's OK, it's optional
        lua_pop(L, 1); // Pop nil
    }

    lua_pop(L, 1); // Pop the plugin table

    loaded = true;
    return std::nullopt;
}

std::variant<std::vector<TextEdit>, PluginError> PluginRuntime::transformSource(const std::string& source, const PluginContext& context)
{
    auto pluginPath = pluginUri.fsPath();

    if (!loaded || !state)
    {
        return PluginError{"Plugin not loaded", pluginPath};
    }

    // If transformSource is not provided, return empty edits
    if (transformSourceRef == LUA_NOREF)
    {
        return std::vector<TextEdit>{};
    }

    lua_State* L = state.get();

    // Set deadline for timeout
    deadline = Luau::TimeTrace::getClock() + (timeoutMs / 1000.0);
    timedOut = false;

    // Push the transformSource function (validated at load time)
    lua_getref(L, transformSourceRef);

    // Push arguments: source string, context table
    lua_pushstring(L, source.c_str());

    // Create context table
    lua_createtable(L, 0, 3);
    lua_pushstring(L, context.filePath.c_str());
    lua_setfield(L, -2, "filePath");
    lua_pushstring(L, context.moduleName.c_str());
    lua_setfield(L, -2, "moduleName");
    lua_pushstring(L, context.languageId.c_str());
    lua_setfield(L, -2, "languageId");

    // Call the function (use pcall, not resume - resume is for coroutines)
    int status = lua_pcall(L, 2, 1, 0);
    if (status != LUA_OK)
    {
        if (timedOut)
        {
            return PluginError{"Plugin transformSource timed out", pluginPath};
        }

        std::string error = lua_isstring(L, -1) ? lua_tostring(L, -1) : "Unknown error";
        lua_pop(L, 1);
        return PluginError{"transformSource failed: " + error, pluginPath};
    }

    // Handle return value
    if (lua_gettop(L) == 0 || lua_isnil(L, -1))
    {
        // No edits - return empty vector
        if (lua_gettop(L) > 0)
            lua_pop(L, 1);
        return std::vector<TextEdit>{};
    }

    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        return PluginError{"transformSource must return a table of edits or nil", pluginPath};
    }

    // Parse the edits table
    std::vector<TextEdit> edits;

    // Iterate over the array
    int tableLen = lua_objlen(L, -1);
    for (int i = 1; i <= tableLen; i++)
    {
        lua_rawgeti(L, -1, i);

        if (!lua_istable(L, -1))
        {
            lua_pop(L, 2); // Pop edit and edits table
            return PluginError{"Each edit must be a table", pluginPath};
        }

        TextEdit edit;

        // Get range
        lua_getfield(L, -1, "range");
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 3); // Pop range, edit, and edits table
            return PluginError{"Edit must have a 'range' table", pluginPath};
        }

        // Get start position
        lua_getfield(L, -1, "start");
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 4);
            return PluginError{"Range must have a 'start' position", pluginPath};
        }

        lua_getfield(L, -1, "line");
        if (!lua_isnumber(L, -1))
        {
            lua_pop(L, 5);
            return PluginError{"Position must have a 'line' number", pluginPath};
        }
        unsigned int startLine = static_cast<unsigned int>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "column");
        if (!lua_isnumber(L, -1))
        {
            lua_pop(L, 5);
            return PluginError{"Position must have a 'column' number", pluginPath};
        }
        unsigned int startColumn = static_cast<unsigned int>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_pop(L, 1); // Pop start table

        // Get end position
        lua_getfield(L, -1, "end");
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 4);
            return PluginError{"Range must have an 'end' position", pluginPath};
        }

        lua_getfield(L, -1, "line");
        if (!lua_isnumber(L, -1))
        {
            lua_pop(L, 5);
            return PluginError{"Position must have a 'line' number", pluginPath};
        }
        unsigned int endLine = static_cast<unsigned int>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "column");
        if (!lua_isnumber(L, -1))
        {
            lua_pop(L, 5);
            return PluginError{"Position must have a 'column' number", pluginPath};
        }
        unsigned int endColumn = static_cast<unsigned int>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_pop(L, 1); // Pop end table
        lua_pop(L, 1); // Pop range table

        edit.range = Luau::Location{{startLine, startColumn}, {endLine, endColumn}};

        // Get newText
        lua_getfield(L, -1, "newText");
        if (!lua_isstring(L, -1))
        {
            lua_pop(L, 3);
            return PluginError{"Edit must have a 'newText' string", pluginPath};
        }
        edit.newText = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_pop(L, 1); // Pop edit table

        edits.push_back(std::move(edit));
    }

    lua_pop(L, 1); // Pop edits table

    return edits;
}

} // namespace Luau::LanguageServer::Plugin
