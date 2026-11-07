# Language Server Clients

## Contributing

We would love to support more language server clients for the Luau Language Server!
If you are interested in adding support, feel free to send in a PR.

## Getting Started

To use the language server from a client, download the latest binary from [GitHub Releases](https://github.com/JohnnyMorganz/luau-lsp/releases).

You can then run the following:

```sh
$ luau-lsp lsp
```

which will start a language server. The server listens to messages following the [Language Server Protocol (LSP)](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/) specification via stdin, and sends messages via stdout.

To find all the command-line options available, run

```sh
$ luau-lsp --help
```

## Configuring Definitions and Documentation

You can add in built-in definitions by passing the `--definitions:@name=PATH` argument.
The `name` should be a unique reference to the definitions file.
This can be done multiple times:

```sh
$ luau-lsp lsp --definitions:@roblox=/path/to/globalTypes.d.luau
```

> NOTE: Definitions file syntax is unstable and undocumented. It may change at any time

For Roblox Users, you can download the Roblox Types Definitions from https://github.com/JohnnyMorganz/luau-lsp/blob/master/scripts/globalTypes.d.luau
(using something like `curl` or `wget` should be sufficient). Multiple definition files are available depending on the
API security level.

Optionally, you can define documentation files as well, by passing `--docs=PATH`.
This provides documentation for any built-in definitions, but is not a requirement.

See https://raw.githubusercontent.com/MaximumADHD/Roblox-Client-Tracker/roblox/api-docs/en-us.json for an example of a
documentation file.

GitHub implements very strong rate limiting for unauthenticated requests. This project provides a Cloudflare-based proxy
to serve static files on a CDN instead. We recommend using the following URLs when downloading:

- `https://luau-lsp.pages.dev/type-definitions/globalTypes.None.d.luau`
- `https://luau-lsp.pages.dev/type-definitions/globalTypes.PluginSecurity.d.luau`
- `https://luau-lsp.pages.dev/type-definitions/globalTypes.LocalUserSecurity.d.luau`
- `https://luau-lsp.pages.dev/type-definitions/globalTypes.RobloxScriptSecurity.d.luau`
- `https://luau-lsp.pages.dev/api-docs/en-us.json`

## Configuring FFlags

The Luau project makes use of FFlags to gate and dynamically enable new features when they are released.

For the Luau Language Server, FFlags can be defined on the command line or by using [initialization options](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#initializeParams):

```sh
$ luau-lsp lsp --flag:NAME=VALUE
```

```json
{
  "initializationOptions": {
    "fflags": {
      "Foo": "True"
    }
  }
}
```

By default, all FFlags are enabled, apart from those that are [defined as experimental](https://github.com/luau-lang/luau/blob/master/Common/include/Luau/ExperimentalFlags.h).
You can disable all FFlags by passing `--no-flags-enabled`.

```sh
$ luau-lsp lsp --no-flags-enabled
```

The VSCode client makes use of the live Roblox Studio FFlag information to sync FFlag state. You can find all current
FFlag values at https://clientsettingscdn.roblox.com/v1/settings/application?applicationName=PCDesktopClient.
Note for simplicity that most relevant FFlags are prefixed by `Luau`. Some FFlags are prefixed by `DebugLuau`, but these
FFlags are experimental and should not typically be enabled.

To view all available FFlags, run:

```sh
$ luau-lsp --show-flags
```

## Optional: Custom `$/command` definition

The Luau Language Server relies on a custom defined LSP notification: `$/command`.
This notification corresponds to the execution of a [VSCode Command](https://code.visualstudio.com/api/references/commands).

Right now, the only command sent is `cursorMove`. This is used during automatic `end` autocompletion, to move the cursor to
the appropriate spot. The following payload is sent in this case:

```json
{
  "command": "cursorMove",
  "data": {
    "to": "prevBlankLine"
  }
}
```

It is optional to decide whether to implement this command for your language client, and the server will run fine without
it being defined. If not available, you may see slight problems when autocompleting `end`.

## Optional: Rename After Refactoring

When resolving refactoring code actions (e.g., Extract to local variable, Extract to function), the server returns a
`CodeAction` with both an `edit` and a `command`. The command triggers a rename prompt so the user can name the newly
created symbol.

The command used is `luau-lsp.rename` with arguments `[uri: string, position: { line: number, character: number }]`.
The VS Code extension handles this by moving the cursor to the specified position and triggering `editor.action.rename`.

For non-VS Code clients, you should register a handler for this command that:

1. Moves the cursor to the given position in the given document
2. Triggers your editor's rename UI

If the command is not handled, the refactoring will still work but the user will need to manually rename the
extracted symbol.

## Optional: Rojo Sourcemap Generation

The Language Server automatically listens for any changes to a `sourcemap.json` file present in the opened workspace root.

It is optional to implement automatic generation of this sourcemap for your language client. It is as simple as running
the following command:

```sh
$ rojo sourcemap --include-non-scripts --watch default.project.json --output sourcemap.json
```

You may wish to make the `default.project.json` file configurable, as well as whether `--include-non-scripts` is enabled.

The Language Server will operate without a sourcemap available, but will not resolve DataModel instances for intellisense.

## Optional: Roblox Studio plugin

A [Roblox Studio Companion Plugin](https://www.roblox.com/library/10913122509/Luau-Language-Server-Companion) is available
for users who would like intellisense for non-Rojo and non-filesystem based DataModel instances.

The companion plugin sends HTTP requests to the following endpoints on localhost at the user-defined port:

- `GET /get-file-paths`
- `POST /full`
- `POST /changes`
- `POST /clear`

The Language Server listens to the following notifications and requests from a language client:

- `$/plugin/full` (notification)
- `$/plugin/changes` (request)
- `$/plugin/clear` (notification)

It is optional to implement support for the companion plugin. This involves creating a HTTP listener on your language
client, which then sends the corresponding LSP notification or request to the server.

The `POST /full` request receives a full DataModel tree with the following body:

```json
{
    "tree": {
        "Id": 1,
        "Session": "string",
        "Name": "string",
        "ClassName": "string",
        "FilePaths": ["string"],
        "Children": [
            ...
        ]
    }
}
```

The `$/plugin/full` LSP notification expects the `tree` property directly sent (i.e., you should send `request.body.tree`).

`FilePaths` and `Children` are omitted when empty.

After sending the full tree, the plugin sends incremental changes to it with `POST /changes`. `Id` identifies an instance,
and `Session` (only set on the root) identifies the full tree that changes apply to. The request has the following body,
and the changes are applied in order:

```json
{
    "session": "string",
    "changes": [
        { "Type": "Add", "Parent": 1, "Instance": { "Id": 2, "Name": "string", "ClassName": "string", "Children": [] } },
        { "Type": "Update", "Id": 2, "Name": "string?", "Parent": 1, "FilePaths": ["string"] },
        { "Type": "Remove", "Id": 2 }
    ]
}
```

`Update` only contains the properties that changed. The body should be sent directly as the `$/plugin/changes` LSP
request. Respond with `200` if the request succeeds. If the request fails (for example, because the language server
restarted and no longer has the full tree), respond with `409`: the plugin then sends the full tree again. If the
endpoint is not implemented, the plugin sends the full tree on every change instead.

Further Reference:

- https://github.com/JohnnyMorganz/luau-lsp/blob/main/plugin/src/init.server.lua
- https://github.com/JohnnyMorganz/luau-lsp/blob/main/src/StudioPlugin.cpp
- https://github.com/JohnnyMorganz/luau-lsp/blob/main/editors/code/src/extension.ts

## Optional: Bytecode generation

The Language server implements support for computing file-level textual bytecode, source code remarks, and codegen instructions for lower level debugging features.

A custom LSP request message is implemented:

- `luau-lsp/bytecode`: `{ textDocument: TextDocumentIdentifier, optimizationLevel: number }`, returns `string` - textual bytecode output
- `luau-lsp/compilerRemarks`: `{ textDocument: TextDocumentIdentifier, optimizationLevel: number }`, returns `string` - source code with inline remarks as comments
- `luau-lsp/codeGen`: `{ textDocument: TextDocumentIdentifier, optimizationLevel: number, codeGenTarget: string }`,
  returns `string` - annotated codegen instructions

`codeGenTarget` can be one of: `"host" | "a64" | "a64_nofeatures" | "x64_windows" | "x64_systemv"`

You can implement this request via a custom command to surface this information in your editor

## Optional: Require Graph

The Language server can generate a require graph from a single file, or of the whole workspace. The require graph visualises dependency links between modules. The require graph is generated in DOT format

A custom LSP request message is implemented:

- `luau-lsp/requireGraph`: `{ textDocument: TextDocumentIdentifier, fromTextDocumentOnly: boolean }`, returns `string` - DOT file output
  - `textDocument`: the text document to generate the require graph for
  - `fromTextDocumentOnly`: whether the require graph should only include the dependencies from the selected text document. If false, the graph includes all indexed modules from the selected text document's workspace

You can implement this request via a custom command to surface this information in your editor. You may need a `dot` visualizer.

## Optional: View Internal Source (Debug)

The language server supports [source code transformation plugins](../src/Plugin/README.md). Plugin configuration is handled through standard client settings and requires no special editor support.

A custom LSP request is available to view the transformed source that Luau type-checks after plugin transformations:

- `luau-lsp/debug/viewInternalSource`: `{ textDocument: TextDocumentIdentifier }`, returns `string` - the transformed source

You can implement this request via a custom command to surface this information in your editor.
