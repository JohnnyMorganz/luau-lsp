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

which will start a language server. The server listens to messages following the Language Server Protocol (LSP)
via stdin, and sends messages via stdout.

To find all the command-line options available, run

```sh
$ luau-lsp --help
```

## Configuring Definitions and Documentation

You can add in built-in definitions by passing the `--definitions=PATH` argument.
This can be done multiple times:

```sh
$ luau-lsp lsp --definitions=/path/to/globalTypes.d.luau
```

For Roblox Users, you can download the Roblox Types Definitions from https://github.com/JohnnyMorganz/luau-lsp/blob/master/scripts/globalTypes.d.lua
(using something like `curl` or `wget` should be sufficient).

Optionally, you can define documentation files as well, by passing `--docs=PATH`.
This provides documentation for any built-in definitions, but is not a requirement.

See https://raw.githubusercontent.com/MaximumADHD/Roblox-Client-Tracker/roblox/api-docs/en-us.json for an example of a
documentation file.
