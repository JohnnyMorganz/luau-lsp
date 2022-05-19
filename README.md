# Luau Language Server

A proof of concept implementation of a language server for the [Luau](https://github.com/Roblox/luau) programming language.

## Getting Started

Install the extension from the marketplace: https://marketplace.visualstudio.com/items?itemName=JohnnyMorganz.luau-lsp

To resolve your instance tree and provide module resolution, the language server uses Rojo sourcemaps.
The language server will automatically create a `sourcemap.json` in your workspace root on startup and whenever files are added/created/renamed.

It does this by running the `rojo sourcemap` command, hence the Rojo executable must be available to use in your workspace root.
(Note: `rojo sourcemap` is currently not released, you must manually install latest Rojo directly using `cargo install rojo --git https://github.com/rojo-rbx/rojo.git` to have sourcemap generation support).
It is recommend to `.gitignore` the `sourcemap.json` file. In future, the language server will generate the file internally.

Note that currently the language server only supports generating a sourcemap from a `default.project.json`. If other project files are necessary, please open an issue.

The extension will also automatically populate the latest API types and documentation.

## Design Goals

The initial goal is to develop a language server supporting all common LSP functions.
Module resolution and typing will initially revolve around [Rojo](https://github.com/JohnnyMorganz/luau-analyze-rojo).

The idea is to ensure module resolution is customisable, allowing the server to later be easily extended to support other environments where Luau may be used.
We could also potentially take it a step forward, allowing the server to be used on an Lua 5.1 codebase through a translation layer (such as type comments through EmmyLua), allowing the language server to support general purpose Lua development powered by the Luau type inference engine.

If you use Luau in a different environment and are interested in using the language server, please get in touch!

## Supported Features

- [x] Rojo Files Resolution
- [x] API Type Definitions
- [x] Diagnostics (incl. type errors)
- [x] Autocompletion
- [x] Hover
- [x] Signature Help
- [x] Go To Definition
- [x] Go To Type Definition
- [ ] Find References
- [ ] Document Highlight
- [x] Document Link
- [ ] Document Symbol
- [ ] Color Provider
- [ ] Rename
- [ ] Folding Range
- [ ] Selection Range
- [ ] Call Hierarchy
- [ ] Type Hierarchy
- [ ] Semantic Tokens
- [ ] Inlay Hints
- [ ] Workspace Symbols

The following are extra features defined in the LSP specification, but most likely do not apply to Luau or are not necessary.
They can be investigated at a later time:

- [ ] Go To Declaration (do not apply)
- [ ] Go To Implementation (do not apply)
- [ ] Code Actions (not necessary - could potentially add "fixers" for lints)
- [ ] Code Lens (not necessary)
- [ ] Inline Value (applies for debuggers only)
- [ ] Moniker
- [ ] Formatting (see [stylua](https://github.com/JohnnyMorganz/StyLua))

## Build From Source

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target luau-lsp --config Release
```
