# Luau Language Server

A proof of concept implementation of a language server for the [Luau](https://github.com/Roblox/luau) programming language.

## Getting Started

Install the extension from the marketplace: https://marketplace.visualstudio.com/items?itemName=JohnnyMorganz.luau-lsp

In order to work effectively, you currently need to manually create a Rojo `sourcemap.json` in your workspace root.
This allows the language server to easily resolve your instance tree and provide module resolution.

Create a sourcemap:

```sh
rojo sourcemap --include-non-scripts default.project.json --output sourcemap.json
```

(Note: `rojo sourcemap` is currently not released, you must manually install latest Rojo directly using `cargo install rojo --git https://github.com/rojo-rbx/rojo.git`)

The extension will automatically populate the latest API types and documentation.

## Design Goals

The initial goal is to develop a language server supporting all common LSP functions.
Module resolution and typing will initially revolve around [Rojo](https://github.com/JohnnyMorganz/luau-analyze-rojo).

The idea is to ensure module resolution is customisable, allowing the server to later be easily extended to support other environments where Luau may be used.
We could also potentially take it a step forward, allowing the server to be used on an Lua 5.1 codebase through a translation layer (such as type comments through EmmyLua), allowing the language server to support general purpose Lua development powered by the Luau type inference engine.

## Supported Features

- [x] Rojo Files Resolution
- [x] API Type Definitions
- [x] Diagnostics (incl. type errors)
- [x] Autocompletion
- [x] Hover
- [x] Signature Help
- [x] Go To Definition
- [ ] Go To Type Definition
- [ ] Find References
- [ ] Document Highlight
- [x] Document Link
- [ ] Document Symbol
- [ ] Code Actions
- [ ] Code Lens
- [ ] Color Provider
- [ ] Formatting
- [ ] Rename
- [ ] Folding Range
- [ ] Selection Range
- [ ] Call Hierarchy
- [ ] Type Hierarchy
- [ ] Semantic Tokens
- [ ] Inline Value
- [ ] Inlay Hints
- [ ] Workspace Symbols

## Build From Source

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target luau-lsp --config Release
```
