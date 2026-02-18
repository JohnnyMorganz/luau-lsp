#pragma once
#include "LSP/Uri.hpp"

#include <string>

struct lua_State;
class WorkspaceFolder;

namespace Luau::LanguageServer::Plugin
{

// Tag for Uri userdata (unique identifier)
constexpr int kUriUserdataTag = 100;

// Tag for LuaApiContext userdata
constexpr int kLuaApiContextTag = 101;

// Context passed to plugin Lua API functions via upvalue
struct LuaApiContext
{
    WorkspaceFolder* workspace;
    std::string pluginPath;
};

// Register the lsp global API and print override
void registerLspApi(lua_State* L, WorkspaceFolder* workspace, const std::string& pluginPath);

// Register Uri userdata metatable
void registerUriUserdata(lua_State* L);

// Helper: push Uri as userdata onto stack
void pushUri(lua_State* L, const Uri& uri);

// Helper: get Uri from stack (validates type, throws on error)
Uri* checkUri(lua_State* L, int idx);

} // namespace Luau::LanguageServer::Plugin
