#pragma once
#include "LSP/Uri.hpp"

struct lua_State;
class WorkspaceFolder;

namespace Luau::LanguageServer::Plugin
{

// Tag for Uri userdata (unique identifier)
constexpr int kUriUserdataTag = 100;

// Register the lsp global API
void registerLspApi(lua_State* L, WorkspaceFolder* workspace);

// Register Uri userdata metatable
void registerUriUserdata(lua_State* L);

// Helper: push Uri as userdata onto stack
void pushUri(lua_State* L, const Uri& uri);

// Helper: get Uri from stack (validates type, throws on error)
Uri* checkUri(lua_State* L, int idx);

} // namespace Luau::LanguageServer::Plugin
