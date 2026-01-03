#include "Plugin/PluginLuaApi.hpp"
#include "LSP/Workspace.hpp"
#include "LuauFileUtils.hpp"

#include "lua.h"
#include "lualib.h"

#include <cstring>

namespace Luau::LanguageServer::Plugin
{

// Forward declarations for internal functions
static int uriIndex(lua_State* L);
static int uriToString(lua_State* L);
static int uriEqual(lua_State* L);
static int uriJoinPath(lua_State* L);

static int lspWorkspaceGetRootUri(lua_State* L);
static int lspFsReadFile(lua_State* L);
static int lspUriParse(lua_State* L);
static int lspUriFile(lua_State* L);

// ============================================================================
// Uri Userdata Implementation
// ============================================================================

void pushUri(lua_State* L, const Uri& uri)
{
    // Allocate userdata for Uri
    void* memory = lua_newuserdatatagged(L, sizeof(Uri), kUriUserdataTag);

    // Construct Uri in-place using placement new
    new (memory) Uri(uri);

    // Set metatable
    luaL_getmetatable(L, "lsp.Uri");
    lua_setmetatable(L, -2);
}

Uri* checkUri(lua_State* L, int idx)
{
    void* ud = lua_touserdatatagged(L, idx, kUriUserdataTag);
    if (!ud)
    {
        luaL_typeerrorL(L, idx, "Uri");
    }
    return static_cast<Uri*>(ud);
}

static int uriDestructor(lua_State* L)
{
    Uri* uri = static_cast<Uri*>(lua_touserdatatagged(L, 1, kUriUserdataTag));
    if (uri)
    {
        uri->~Uri();
    }
    return 0;
}

static int uriIndex(lua_State* L)
{
    Uri* uri = checkUri(L, 1);
    const char* key = luaL_checkstring(L, 2);

    // Properties
    if (strcmp(key, "scheme") == 0)
    {
        lua_pushstring(L, uri->scheme.c_str());
    }
    else if (strcmp(key, "authority") == 0)
    {
        lua_pushstring(L, uri->authority.c_str());
    }
    else if (strcmp(key, "path") == 0)
    {
        lua_pushstring(L, uri->path.c_str());
    }
    else if (strcmp(key, "query") == 0)
    {
        lua_pushstring(L, uri->query.c_str());
    }
    else if (strcmp(key, "fragment") == 0)
    {
        lua_pushstring(L, uri->fragment.c_str());
    }
    else if (strcmp(key, "fsPath") == 0)
    {
        lua_pushstring(L, uri->fsPath().c_str());
    }
    // Methods
    else if (strcmp(key, "joinPath") == 0)
    {
        lua_pushcfunction(L, uriJoinPath, "joinPath");
    }
    else if (strcmp(key, "toString") == 0)
    {
        lua_pushcfunction(L, uriToString, "toString");
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

static int uriToString(lua_State* L)
{
    Uri* uri = checkUri(L, 1);
    lua_pushstring(L, uri->toString().c_str());
    return 1;
}

static int uriEqual(lua_State* L)
{
    Uri* a = checkUri(L, 1);
    Uri* b = checkUri(L, 2);
    lua_pushboolean(L, *a == *b);
    return 1;
}

static int uriJoinPath(lua_State* L)
{
    Uri* uri = checkUri(L, 1);

    // Get number of path segments
    int nargs = lua_gettop(L);
    if (nargs < 2)
    {
        luaL_errorL(L, "joinPath requires at least one path segment");
    }

    // Build the combined path
    Uri result = *uri;
    for (int i = 2; i <= nargs; i++)
    {
        const char* segment = luaL_checkstring(L, i);
        result = result.resolvePath(segment);
    }

    pushUri(L, result);
    return 1;
}

void registerUriUserdata(lua_State* L)
{
    // Create metatable for Uri userdata
    luaL_newmetatable(L, "lsp.Uri");

    lua_pushcfunction(L, uriIndex, "__index");
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, uriToString, "__tostring");
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, uriEqual, "__eq");
    lua_setfield(L, -2, "__eq");

    lua_pushcfunction(L, uriDestructor, "__gc");
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1); // Pop metatable
}

// ============================================================================
// lsp API Implementation
// ============================================================================

static int lspWorkspaceGetRootUri(lua_State* L)
{
    auto* workspace = static_cast<WorkspaceFolder*>(lua_touserdata(L, lua_upvalueindex(1)));
    pushUri(L, workspace->rootUri);
    return 1;
}

static int lspFsReadFile(lua_State* L)
{
    auto* workspace = static_cast<WorkspaceFolder*>(lua_touserdata(L, lua_upvalueindex(1)));

    // Check if filesystem access is enabled
    auto config = workspace->client->getConfiguration(workspace->rootUri);
    if (!config.plugins.fileSystem.enabled)
    {
        luaL_errorL(L, "filesystem access not available");
    }

    // Get Uri argument (must be Uri userdata)
    Uri* targetUri = checkUri(L, 1);

    // Validate scheme
    if (targetUri->scheme != "file")
    {
        luaL_errorL(L, "only file:// URIs are supported");
    }

    // Security: check if within workspace
    if (!workspace->rootUri.isAncestorOf(*targetUri))
    {
        luaL_errorL(L, "access denied: file is outside workspace");
    }

    // Read file
    auto content = Luau::FileUtils::readFile(targetUri->fsPath());
    if (!content)
    {
        luaL_errorL(L, "file not found or cannot be read");
    }

    lua_pushlstring(L, content->c_str(), content->size());
    return 1;
}

static int lspUriParse(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    pushUri(L, Uri::parse(str));
    return 1;
}

static int lspUriFile(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    pushUri(L, Uri::file(path));
    return 1;
}

void registerLspApi(lua_State* L, WorkspaceFolder* workspace)
{
    // Create lsp global table
    lua_newtable(L);

    // lsp.workspace table
    lua_newtable(L);
    lua_pushlightuserdata(L, workspace);
    lua_pushcclosure(L, lspWorkspaceGetRootUri, "getRootUri", 1);
    lua_setfield(L, -2, "getRootUri");
    lua_setfield(L, -2, "workspace");

    // lsp.fs table
    lua_newtable(L);
    lua_pushlightuserdata(L, workspace);
    lua_pushcclosure(L, lspFsReadFile, "readFile", 1);
    lua_setfield(L, -2, "readFile");
    lua_setfield(L, -2, "fs");

    // lsp.Uri table (constructor functions)
    lua_newtable(L);
    lua_pushcfunction(L, lspUriParse, "parse");
    lua_setfield(L, -2, "parse");
    lua_pushcfunction(L, lspUriFile, "file");
    lua_setfield(L, -2, "file");
    lua_setfield(L, -2, "Uri");

    // Set as global
    lua_setglobal(L, "lsp");
}

} // namespace Luau::LanguageServer::Plugin
