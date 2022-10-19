# Luau Language Server

An implementation of a language server for the [Luau](https://github.com/Roblox/luau) programming language.

## Getting Started

Install the extension from the marketplace: https://marketplace.visualstudio.com/items?itemName=JohnnyMorganz.luau-lsp

### For General Users

The language server should be immediately usable for general Luau code after installation.
String require support is provided for module paths, using `require("module")`.
Note that the file is searched **relative to the workspace root** (equivalent to the [command-line REPL](https://github.com/Roblox/luau#usage) `luau`).

Type definitions can be provided by configuring `luau-lsp.types.definitionFiles`.

If there are specific features you require in the language server for your use case, feel free to open an issue.

### For Rojo Users

Rojo instance tree and requiring support is provided by default, and the language server should be able to directly emulate Studio.
The extension will automatically populate the latest API types and documentation (which can be disabled by configuring `luau-lsp.types.roblox`).

To resolve your instance tree and provide module resolution, the language server uses Rojo sourcemaps.
The language server will automatically create a `sourcemap.json` in your workspace root on startup and whenever files are added/created/renamed.

It does this by running the `rojo sourcemap` command, hence Rojo 7.1.0+ must be available to execute in your workspace root.
It is recommend to `.gitignore` the `sourcemap.json` file. In future, the language server will generate the file internally.

Note, if you are using the VSCode extension on macOS, you need to configure the location of the Rojo binary at `luau-lsp.sourcemap.rojoPath`.

By default we generate a sourcemap for a `default.project.json` project file. The name can be changed in extension settings, as well as whether non-script instances are included in the sourcemap (included by default). Autogeneration of sourcemaps can also be toggled completely on/off in settings - the server will instead just listen to manual changes to `sourcemap.json` files.

> Note: in the diagnostics type checker, the types for DataModel (DM) instances will resolve to `any`. This is a current limitation to reduce false positives.
> However, autocomplete and hover intellisense will correctly resolve the DM type. [Read more](https://github.com/JohnnyMorganz/luau-lsp/issues/83#issuecomment-1192865024)

#### A companion Studio plugin is available to provide DataModel information for Instances which are not part of your Rojo build / filetree: [Plugin Marketplace](https://www.roblox.com/library/10913122509/Luau-Language-Server-Companion)

## Standalone

The tool can run standalone, similar to [`luau-analyze`](https://github.com/JohnnyMorganz/luau-analyze-rojo), to provide type and lint warnings in CI, with full Rojo resolution and API types support.
The entry point for the analysis tool is `luau-lsp analyze`.

Install the binary and run `luau-lsp --help` for more information.

## Design Goals

The initial goal is to develop a language server supporting all common LSP functions.
Module resolution and typing will initially revolve around [Rojo](https://github.com/JohnnyMorganz/luau-analyze-rojo).

The idea is to ensure module resolution is customisable, allowing the server to later be easily extended to support other environments where Luau may be used.
We could also potentially take it a step forward, allowing the server to be used on an Lua 5.1 codebase through a translation layer (such as type comments through EmmyLua), allowing the language server to support general purpose Lua development powered by the Luau type inference engine.

If you use Luau in a different environment and are interested in using the language server, please get in touch!

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
- [ ] Color Provider
- [x] Rename
- [ ] Call Hierarchy
- [x] Semantic Tokens
- [x] Inlay Hints
- [ ] Workspace Symbols

The following are extra features defined in the LSP specification, but most likely do not apply to Luau or are not necessary.
They can be investigated at a later time:

- [ ] Go To Declaration (do not apply)
- [ ] Go To Implementation (do not apply)
- [ ] Code Actions (not necessary - could potentially add "fixers" for lints)
- [ ] Code Lens (not necessary)
- [ ] Document Highlight (not necessary - editor highlighting is sufficient)
- [ ] Folding Range (not necessary - editor folding is sufficient)
- [ ] Selection Range (not necessary - editor selection is sufficient)
- [ ] Inline Value (applies for debuggers only)
- [ ] Moniker
- [ ] Formatting (see [stylua](https://github.com/JohnnyMorganz/StyLua))
- [ ] Type Hierarchy (Luau currently does not provide any [public] ways to define type hierarchies)

## Build From Source

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target luau-lsp --config Release
```
