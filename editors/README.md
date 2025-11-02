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
$ luau-lsp lsp --definition:@roblox=/path/to/globalTypes.d.luau
```

> NOTE: Definitions file syntax is unstable and undocumented. It may change at any time

For Roblox Users, you can download the Roblox Types Definitions from https://github.com/JohnnyMorganz/luau-lsp/blob/master/scripts/globalTypes.d.luau
(using something like `curl` or `wget` should be sufficient).

Optionally, you can define documentation files as well, by passing `--docs=PATH`.
This provides documentation for any built-in definitions, but is not a requirement.

See https://raw.githubusercontent.com/MaximumADHD/Roblox-Client-Tracker/roblox/api-docs/en-us.json for an example of a
documentation file.

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
for users who would like intellisense for non-filesystem based DataModel instances.

The companion plugin sends HTTP post requests to the following endpoints on localhost at the user-defined port:

- `POST /full`
- `POST /clear`

The Language Server listens to the following notifications from a language client:

- `$/plugin/full`
- `$/plugin/clear`

It is optional to implement support for the companion plugin. This involves creating a HTTP listener on your language
client, which then sends the corresponding LSP notification to the server.

The `POST /full` request receives a full DataModel tree with the following body:

```json
{
    "tree": {
        "Name": "string",
        "ClassName": "string",
        "Children": {
            ...
        }
    }
}
```

The `$/plugin/full` LSP notification expects the `tree` property directly sent (i.e., you should send `request.body.tree`).

Further Reference:

- https://github.com/JohnnyMorganz/luau-lsp/blob/main/plugin/src/init.server.lua
- https://github.com/JohnnyMorganz/luau-lsp/blob/main/src/StudioPlugin.cpp
- https://github.com/JohnnyMorganz/luau-lsp/blob/main/editors/code/src/extension.ts

## Optional: Bytecode generation

The Language server implements support for computing file-level textual bytecode and source code remarks, for lower level debugging features.

A custom LSP request message is implemented:

- `luau-lsp/bytecode`: `{ textDocument: TextDocumentIdentifier, optimizationLevel: number }`, returns `string` - textual bytecode output
- `luau-lsp/compilerRemarks`: `{ textDocument: TextDocumentIdentifier, optimizationLevel: number }`, returns `string` - source code with inline remarks as comments

You can implement this request via a custom command to surface this information in your editor

## Optional: Require Graph

The Language server can generate a require graph from a single file, or of the whole workspace. The require graph visualises dependency links between modules. The require graph is generated in DOT format

A custom LSP request message is implemented:

- `luau-lsp/requireGraph`: `{ textDocument: TextDocumentIdentifier, fromTextDocumentOnly: boolean }`, returns `string` - DOT file output
  - `textDocument`: the text document to generate the require graph for
  - `fromTextDocumentOnly`: whether the require graph should only include the dependencies from the selected text document. If false, the graph includes all indexed modules from the selected text document's workspace

You can implement this request via a custom command to surface this information in your editor. You may need a `dot` visualizer.
