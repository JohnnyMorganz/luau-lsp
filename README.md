# Luau Language Server

An implementation of a language server for the [Luau](https://github.com/Roblox/luau) programming language.

## Getting Started

Install the extension from the VSCode Marketplace or OpenVSX Registry:

- VSCode Marketplace: https://marketplace.visualstudio.com/items?itemName=JohnnyMorganz.luau-lsp
- OpenVSX Registry: https://open-vsx.org/extension/JohnnyMorganz/luau-lsp

Alternatively, checkout [Getting Started for Language Server Clients](https://github.com/JohnnyMorganz/luau-lsp/blob/main/editors/README.md)
to setup your own client for a different editor

### For General Users

The language server should be immediately usable for general Luau code after installation.
String require support is provided for module paths, using `require("module")`.
There are two options for resolving requires, which can be configured using `luau-lsp.require.mode`:
`relativeToWorkspaceRoot` (the default - equivalent to the [command-line REPL](https://github.com/Roblox/luau#usage) `luau`)
or `relativeToFile`.

Type definitions can be provided by configuring `luau-lsp.types.definitionFiles`, with corresponding
documentation added using `luau-lsp.types.documentationFiles`.

If you use Luau in a different environment and are interested in using the language server, or
looking for any specific features, please get in touch!

### For Rojo Users

Rojo instance tree and requiring support is provided by default, and the language server should be able to directly emulate Studio.
The extension will automatically populate the latest API types and documentation (which can be disabled by configuring `luau-lsp.types.roblox`).

To resolve your instance tree and provide module resolution, the language server uses Rojo-style sourcemaps.
The language server will automatically create a `sourcemap.json` in your workspace root on startup and whenever files are added/created/renamed.

It does this by running the `rojo sourcemap` command, hence Rojo 7.1.0+ must be available to execute in your workspace root.
It is recommend to `.gitignore` the `sourcemap.json` file. In future, the language server will generate the file internally.

Note, if you are using the VSCode extension on macOS, you need to configure the location of the Rojo binary at `luau-lsp.sourcemap.rojoPath`.

By default we generate a sourcemap for a `default.project.json` project file. The name can be changed in extension settings, as well as whether non-script instances are included in the sourcemap (included by default). Autogeneration of sourcemaps can also be toggled completely on/off in settings - the server will instead just listen to manual changes to `sourcemap.json` files.

If you do not use Rojo, you can still use the Luau Language Server, you just need to manually generate a `sourcemap.json`
file for your particular project layout.

> Note: in the diagnostics type checker, the types for DataModel (DM) instances will resolve to `any`. This is a current limitation to reduce false positives.
> However, autocomplete and hover intellisense will correctly resolve the DM type.
> To enable this mode for diagnostics, set `luau-lsp.diagnostics.strictDatamodelTypes` (off by default).
> [Read more](https://github.com/JohnnyMorganz/luau-lsp/issues/83#issuecomment-1192865024).

**A companion Studio plugin is available to provide DataModel information for Instances which are not part of your Rojo build / filetree: [Plugin Marketplace](https://www.roblox.com/library/10913122509/Luau-Language-Server-Companion)**

## Standalone

The tool can run standalone, similar to [`luau-analyze`](https://github.com/JohnnyMorganz/luau-analyze-rojo), to provide type and lint warnings in CI, with full Rojo resolution and API types support.
The entry point for the analysis tool is `luau-lsp analyze`.

Install the binary and run `luau-lsp --help` for more information.

## Supported Features

- [x] Diagnostics (incl. type errors)
- [x] Autocompletion
- [x] Hover
- [x] Signature Help
- [x] Go To Definition
- [x] Go To Type Definition
- [x] Find References
- [x] Document Link
- [x] Document Symbol
- [x] Color Provider
- [x] Rename
- [x] Semantic Tokens
- [x] Inlay Hints
- [x] Documentation Comments ([Moonwave Style](https://github.com/evaera/moonwave) - supporting both `--- comment` and `--[=[ comment ]=]`, but must be next to statement)
- [x] Code Actions
- [x] Workspace Symbols
- [x] Folding Range
- [x] Call Hierarchy

The following are extra features defined in the LSP specification, but most likely do not apply to Luau or are not necessary.
They can be investigated at a later time:

- [ ] Go To Declaration (do not apply)
- [ ] Go To Implementation (do not apply)
- [ ] Code Lens (not necessary)
- [ ] Document Highlight (not necessary - editor highlighting is sufficient)
- [ ] Selection Range (not necessary - editor selection is sufficient)
- [ ] Inline Value (applies for debuggers only)
- [ ] Moniker
- [ ] Formatting (see [stylua](https://github.com/JohnnyMorganz/StyLua))
- [ ] Type Hierarchy (Luau currently does not provide any [public] ways to define type hierarchies)

## Build From Source

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target Luau.LanguageServer.CLI --config Release
```
