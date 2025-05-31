#include "LSP/PluginManager.hpp"

#include "Luau/Compiler.h"

namespace Plugins
{
static void writestring(const char* s, size_t l)
{
    fwrite(s, 1, l, stderr);
}

static int luaB_print(lua_State* L)
{
    int n = lua_gettop(L); // number of arguments
    for (int i = 1; i <= n; i++)
    {
        size_t l;
        const char* s = luaL_tolstring(L, i, &l); // convert to string using __tostring et al
        if (i > 1)
            writestring("\t", 1);
        writestring(s, l);
        lua_pop(L, 1); // pop result
    }
    writestring("\n", 1);
    return 0;
}

PluginManager::PluginManager(Client* client)
    : client(client)
    , globalState(luaL_newstate(), lua_close)
{
    GL = globalState.get();
    luaL_openlibs(GL);

    // Redefine print to go to stderr
    lua_pushcfunction(GL, luaB_print, "print");
    lua_setglobal(GL, "print");

    luaL_sandbox(GL);
}

static Luau::CompileOptions copts()
{
    Luau::CompileOptions result = {};
    result.optimizationLevel = 1;
    result.debugLevel = 1;
    result.typeInfoLevel = 1;
    result.coverageLevel = 0;

    return result;
}

void PluginManager::registerPlugin(const std::string& name)
{
    client->sendTrace("plugins: loading " + name);

    std::optional<std::string> source = readFile(name);
    if (!source)
    {
        client->sendLogMessage(lsp::MessageType::Error, "Failed to register plugin '" + name + "', file not found");
        return;
    }

    lua_State* L = lua_newthread(GL);
    luaL_sandboxthread(L);

    std::string chunkname = "@" + name; //+ normalizePath(name);
    std::string bytecode = Luau::compile(*source, copts());
    int status = 0;

    if (luau_load(L, chunkname.c_str(), bytecode.data(), bytecode.size(), 0) == 0)
    {
        //        if (codegen)
        //        {
        //            Luau::CodeGen::CompilationOptions nativeOptions;
        //            Luau::CodeGen::compile(L, -1, nativeOptions);
        //        }

        status = lua_resume(L, NULL, 0);
    }
    else
    {
        status = LUA_ERRSYNTAX;
    }

    if (status == 0)
    {
        plugins.emplace_back(Plugin{L});
    }
    else
    {
        // TODO: report error properly
        std::string error;

        if (status == LUA_YIELD)
        {
            error = "thread yielded unexpectedly";
        }
        else if (const char* str = lua_tostring(L, -1))
        {
            error = str;
        }

        error += "\nstacktrace:\n";
        error += lua_debugtrace(L);

        fprintf(stderr, "%s", error.c_str());
    }

    lua_pop(GL, 1);
    //    return status == 0;
}

std::optional<std::string> PluginManager::handleReadFile(Plugins::OnReadFileContext ctx)
{
    std::optional<std::string> result = std::nullopt;

    // TODO: build over multiple plugins, rather than last plugin wins
    for (const auto& plugin : plugins)
    {
        client->sendTrace("executing plugin");

        lua_rawcheckstack(plugin.L, 3);

        lua_getglobal(plugin.L, "OnReadFile");
        if (!lua_isnil(plugin.L, -1))
        {
            client->sendTrace("plugin defines OnReadFile");

            lua_createtable(plugin.L, 0, 3);

            lua_pushstring(plugin.L, ctx.uri.toString().c_str());
            lua_setfield(plugin.L, -2, "uri");

            lua_pushstring(plugin.L, ctx.moduleName.c_str());
            lua_setfield(plugin.L, -2, "moduleName");

            lua_pushlstring(plugin.L, ctx.contents.data(), ctx.contents.size());
            lua_setfield(plugin.L, -2, "contents");

            if (lua_pcall(plugin.L, 1, 1, 0) != 0)
                client->sendLogMessage(lsp::MessageType::Error, "plugin OnReadFile failed");

            if (lua_isstring(plugin.L, -1))
                result = lua_tostring(plugin.L, -1);
            else if (lua_isnil(plugin.L, -1))
                result = std::nullopt;
            else
                client->sendLogMessage(lsp::MessageType::Error, "plugin OnReadFile must return string or nil");

            lua_pop(plugin.L, 1);
        }
        else
            lua_pop(plugin.L, 1);
    }

    return result;
}

}