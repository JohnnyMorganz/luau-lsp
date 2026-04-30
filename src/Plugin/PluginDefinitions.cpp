#include "Plugin/PluginDefinitions.hpp"

namespace Luau::LanguageServer::Plugin
{

const char* LSPPLUGIN_DEFINITIONS = R"TYPES(
declare extern type Uri with
    scheme: string
    authority: string
    path: string
    query: string
    fragment: string
    fsPath: string
    function joinPath(self, ...: string): Uri
    function toString(self): string
end

declare lsp: {
    workspace: {
        getRootUri: () -> Uri,
    },
    fs: {
        readFile: (uri: Uri) -> string,
        exists: (uri: Uri) -> boolean,
        listDirectory: (uri: Uri) -> {Uri},
    },
    client: {
        sendLogMessage: (type: "error" | "warning" | "info" | "log", message: string) -> (),
    },
    Uri: {
        parse: (uriString: string) -> Uri,
        file: (fsPath: string) -> Uri,
    },
    json: {
        deserialize: (jsonString: string) -> any,
    },
}

export type TextEdit = {
    startLine: number,      -- 1-indexed, inclusive
    startColumn: number,    -- 1-indexed, inclusive, UTF-8 byte offset
    endLine: number,        -- 1-indexed, inclusive
    endColumn: number,      -- 1-indexed, exclusive, UTF-8 byte offset
    newText: string,
}

export type PluginContext = {
    filePath: string,       -- absolute filesystem path of the source file
    moduleName: string,     -- Luau module name (virtual path for Roblox, filesystem path otherwise)
}

export type PluginApi = {
    transformSource: ((source: string, context: PluginContext) -> {TextEdit}?)?,
}
)TYPES";

} // namespace Luau::LanguageServer::Plugin
