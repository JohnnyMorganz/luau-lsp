# Luau Language Server

A proof of concept implementation of a language server for the [Luau](https://github.com/Roblox/luau) programming language.

## Main Repository

For more information, check out the main repository: https://github.com/JohnnyMorganz/luau-lsp

## Requirements

In order to work effectively, the language server currently needs the following two files present in your workspace root:

- [globalTypes.d.lua](https://github.com/JohnnyMorganz/luau-analyze-rojo/blob/master/globalTypes.d.lua)
- `sourcemap.json` (generated using Rojo)
