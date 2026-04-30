# Platform

Your platform determines the standard global type definitions available in your environment, as well as how files are resolved.

You can change this at any time by configuring `luau-lsp.platform.type`

## Standard

This mode will only load the built-in Luau globals into the type checker. You can extend this further with your own global type definitions.

For requiring files, this follow Luau's [require-by-string semantics](https://rfcs.luau.org/new-require-by-string-semantics.html)

```lua
local library = require("../library")
```

## Roblox (default)

The Roblox platform injects all of Roblox's global types into your environment.

This adds support for instance-based requires. A sourcemap is used to map from file-system paths to Roblox DataModel paths

```lua
local library = require(game.ReplicatedStorage.Library)
```
