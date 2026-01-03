#include "Plugin/PluginLuaApi.hpp"
#include "LSP/Workspace.hpp"
#include "LuauFileUtils.hpp"

#include "nlohmann/json.hpp"

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
static int lspClientSendLogMessage(lua_State* L);
static int lspJsonDeserialize(lua_State* L);
static int pluginPrint(lua_State* L);

// Helper to push JSON value onto Lua stack
static void pushJsonValue(lua_State* L, const nlohmann::json& value);

// Helper to get LuaApiContext from upvalue
static LuaApiContext* getLuaApiContext(lua_State* L)
{
    return static_cast<LuaApiContext*>(lua_touserdatatagged(L, lua_upvalueindex(1), kLuaApiContextTag));
}

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
    auto* ctx = getLuaApiContext(L);
    pushUri(L, ctx->workspace->rootUri);
    return 1;
}

static int lspFsReadFile(lua_State* L)
{
    auto* ctx = getLuaApiContext(L);

    // Check if filesystem access is enabled
    auto config = ctx->workspace->client->getConfiguration(ctx->workspace->rootUri);
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
    if (!ctx->workspace->rootUri.isAncestorOf(*targetUri))
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

static int lspClientSendLogMessage(lua_State* L)
{
    auto* ctx = getLuaApiContext(L);

    // Get type argument as string ("error", "warning", "info", "log")
    const char* typeStr = luaL_checkstring(L, 1);
    lsp::MessageType type;
    if (strcmp(typeStr, "error") == 0)
        type = lsp::MessageType::Error;
    else if (strcmp(typeStr, "warning") == 0)
        type = lsp::MessageType::Warning;
    else if (strcmp(typeStr, "info") == 0)
        type = lsp::MessageType::Info;
    else if (strcmp(typeStr, "log") == 0)
        type = lsp::MessageType::Log;
    else
        luaL_errorL(L, "invalid message type '%s' (must be 'error', 'warning', 'info', or 'log')", typeStr);

    // Get message argument
    const char* message = luaL_checkstring(L, 2);

    // Format with prefix and send
    std::string prefixedMessage = "[Plugin " + ctx->pluginPath + "] " + message;
    ctx->workspace->client->sendLogMessage(type, prefixedMessage);

    return 0;
}

static int pluginPrint(lua_State* L)
{
    auto* ctx = getLuaApiContext(L);

    // Concatenate all arguments (like standard print)
    std::string message;
    int n = lua_gettop(L);
    for (int i = 1; i <= n; i++)
    {
        if (i > 1)
            message += "\t";
        message += luaL_tolstring(L, i, nullptr);
        lua_pop(L, 1); // pop the string from luaL_tolstring
    }

    // Format with prefix and send as Info
    std::string prefixedMessage = "[Plugin " + ctx->pluginPath + "] " + message;
    ctx->workspace->client->sendLogMessage(lsp::MessageType::Info, prefixedMessage);

    return 0;
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

// ============================================================================
// JSON API Implementation
// ============================================================================

static void pushJsonValue(lua_State* L, const nlohmann::json& value)
{
    if (value.is_null())
    {
        lua_pushnil(L);
    }
    else if (value.is_boolean())
    {
        lua_pushboolean(L, value.get<bool>());
    }
    else if (value.is_number_integer())
    {
        lua_pushinteger(L, value.get<int64_t>());
    }
    else if (value.is_number())
    {
        lua_pushnumber(L, value.get<double>());
    }
    else if (value.is_string())
    {
        const auto& str = value.get<std::string>();
        lua_pushlstring(L, str.c_str(), str.size());
    }
    else if (value.is_array())
    {
        lua_createtable(L, static_cast<int>(value.size()), 0);
        int index = 1;
        for (const auto& elem : value)
        {
            pushJsonValue(L, elem);
            lua_rawseti(L, -2, index++);
        }
    }
    else if (value.is_object())
    {
        lua_createtable(L, 0, static_cast<int>(value.size()));
        for (const auto& [key, val] : value.items())
        {
            lua_pushlstring(L, key.c_str(), key.size());
            pushJsonValue(L, val);
            lua_rawset(L, -3);
        }
    }
}

static int lspJsonDeserialize(lua_State* L)
{
    size_t len;
    const char* jsonStr = luaL_checklstring(L, 1, &len);

    try
    {
        auto json = nlohmann::json::parse(jsonStr, jsonStr + len);
        pushJsonValue(L, json);
        return 1;
    }
    catch (const nlohmann::json::exception& e)
    {
        luaL_errorL(L, "JSON parse error: %s", e.what());
    }
}

void registerLspApi(lua_State* L, WorkspaceFolder* workspace, const std::string& pluginPath)
{
    // Create LuaApiContext userdata (persists for lifetime of Lua state)
    void* memory = lua_newuserdatatagged(L, sizeof(LuaApiContext), kLuaApiContextTag);
    new (memory) LuaApiContext{workspace, pluginPath};
    int contextIdx = lua_gettop(L);

    // Helper lambda to push context as upvalue and create closure
    auto pushClosureWithContext = [L, contextIdx](lua_CFunction fn, const char* name) {
        lua_pushvalue(L, contextIdx); // Push copy of context userdata
        lua_pushcclosure(L, fn, name, 1);
    };

    // Create lsp global table
    lua_newtable(L);

    // lsp.workspace table
    lua_newtable(L);
    pushClosureWithContext(lspWorkspaceGetRootUri, "getRootUri");
    lua_setfield(L, -2, "getRootUri");
    lua_setfield(L, -2, "workspace");

    // lsp.fs table
    lua_newtable(L);
    pushClosureWithContext(lspFsReadFile, "readFile");
    lua_setfield(L, -2, "readFile");
    lua_setfield(L, -2, "fs");

    // lsp.client table
    lua_newtable(L);
    pushClosureWithContext(lspClientSendLogMessage, "sendLogMessage");
    lua_setfield(L, -2, "sendLogMessage");
    lua_setfield(L, -2, "client");

    // lsp.Uri table (constructor functions - no context needed)
    lua_newtable(L);
    lua_pushcfunction(L, lspUriParse, "parse");
    lua_setfield(L, -2, "parse");
    lua_pushcfunction(L, lspUriFile, "file");
    lua_setfield(L, -2, "file");
    lua_setfield(L, -2, "Uri");

    // lsp.json table (no context needed)
    lua_newtable(L);
    lua_pushcfunction(L, lspJsonDeserialize, "deserialize");
    lua_setfield(L, -2, "deserialize");
    lua_setfield(L, -2, "json");

    // Set lsp as global
    lua_setglobal(L, "lsp");

    // Override print function
    pushClosureWithContext(pluginPrint, "print");
    lua_setglobal(L, "print");

    // Pop the context userdata (it's now referenced by closures)
    lua_pop(L, 1);
}

} // namespace Luau::LanguageServer::Plugin
