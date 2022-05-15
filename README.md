# Luau Language Server

A proof of concept implementation of a language server for the [Luau](https://github.com/Roblox/luau) programming language.

## Design Goals

The initial goal is to develop a language server supporting all common LSP functions.
Module resolution and typing will initially revolve around [Rojo](https://github.com/JohnnyMorganz/luau-analyze-rojo).
The idea is to ensure module resolution is modular, allowing the server to later be extended to support other environments where Luau may be used.
We could also potentially take it a step forward, allowing the server to be used on an Lua 5.1 codebase through a translation layer (such as type comments through EmmyLua), allowing the language server to become general purpose, powered by the Luau type inference engine.

## Initial Stages

- [x] Develop a serial language server which correctly handles JSON-RPC messages, communicating over stdio
- [x] Implement diagnostics based on luau-analyze
- [x] Implement module resolution (based on [luau-analyze-rojo](https://github.com/JohnnyMorganz/luau-analyze-rojo))
- [x] Support `textDocument/completion` using the Luau autocomplete engine

## Supported Features

- [x] Diagnostics (incl. type errors)
- [x] Autocompletion
- [x] Hover
- [x] Signature Help
- [] Go To Declaration
- [] Go To Definition
- [] Go To Type Definition
- [] Go To Implementation
- [] Find References
- [] Document Highlight
- [x] Document Link
- [] Document Symbol
- [] Code Actions
- [] Code Lens
- [] Color Provider
- [] Formatting
- [] Rename
- [] Folding Range
- [] Selection Range
- [] Call Hierarchy
- [] Type Hierarchy
- [] Semantic Tokens
- [] Inline Value
- [] Inlay Hints
- [] Workspace Symbols

## Build From Source

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target luau-lsp --config Release
```
