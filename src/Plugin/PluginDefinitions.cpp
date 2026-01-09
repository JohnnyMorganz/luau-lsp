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

export type Position = {
    line: number,
    column: number,
}

export type Range = {
    start: Position,
    ["end"]: Position,
}

export type TextEdit = {
    range: Range,
    newText: string,
}

export type PluginContext = {
    filePath: string,
    moduleName: string,
    languageId: string,
}

export type PluginApi = {
    transformSource: ((source: string, context: PluginContext) -> {TextEdit}?)?,
}
)TYPES";

} // namespace Luau::LanguageServer::Plugin
