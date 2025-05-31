#pragma once
#include <string>

#include "../../../luau/VM/include/lua.h"
#include "../../../luau/VM/include/lualib.h"

#include "Luau/Module.h"

#include "LSP/Client.hpp"

namespace Plugins
{

struct OnReadFileContext
{
    Uri uri;
    Luau::ModuleName moduleName;
    std::string_view contents;
};

struct Plugin
{
    lua_State* L;
};

class PluginManager
{
public:
    PluginManager(Client* client);

    void registerPlugin(const std::string& name);

    std::optional<std::string> handleReadFile(OnReadFileContext ctx);

private:
    Client* client;

    std::unique_ptr<lua_State, void (*)(lua_State*)> globalState;
    lua_State* GL; // Shorthand alias for 'globalState'

    std::vector<Plugin> plugins;
};

}


// PluginManager
//  - Read luau-lsp.plugins.paths
//  - Register each plugin
// Plugin API:
//
//  function OnReadFile(ctx)
//      - ctx.uri: source URI file
//      - ctx.moduleName: resolved module name of src file
//      - ctx.contents: source file contents
//  end
//
//  function ResolveStringRequire(ctx)
//      - ctx.uri: source URI file
//      - ctx.moduleName: resolved module name of src file
//      Returns:
//          - string: resolved module name of file to read
//          - nil: no special resolution
//  end