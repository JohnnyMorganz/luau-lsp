# Plugin System - Implementation Notes

This file provides implementation guidance for Claude Code when working on the plugin system.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Editor                                    │
│                  (shows original source)                         │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    WorkspaceFileResolver                         │
│                                                                  │
│  1. getTextDocument(uri) called                                  │
│  2. If plugins configured:                                       │
│     - All plugins transform original source (parallel)           │
│     - Edits combined, overlap check performed                    │
│     - PluginTextDocument created with SourceMapping              │
│  3. Returns PluginTextDocument (or original if no plugins)       │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    PluginTextDocument                            │
│                                                                  │
│  - Inherits from TextDocument                                    │
│  - getText() returns transformed source                          │
│  - convertPosition() maps between original and transformed       │
│  - LSP operations work transparently                             │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Luau Frontend                               │
│                  (type checks transformed source)                │
└─────────────────────────────────────────────────────────────────┘
```

## Components

### PluginTypes (`Plugin/PluginTypes.hpp`)

Simple data structure representing a text edit with a range and replacement text. Uses Luau's `Location` type for the range.

### SourceMapping (`Plugin/SourceMapping.cpp`)

Core position mapping between original and transformed source:

- Built from a list of `TextEdit`s
- Provides bidirectional position conversion
- Validates edits don't overlap (throws on overlap)
- Sorts edits by position before applying

### PluginTextDocument (`Plugin/PluginTextDocument.cpp`)

Inherits from `TextDocument`, with the base class holding **transformed** content (what Luau sees). An internal `TextDocument` member holds the **original** content (what the user sees):

- Base `getText()`, `getLineOffsets()`, etc. operate on transformed content automatically
- `convertPosition()` uses the internal original document for UTF-16 conversion, then maps through SourceMapping
- LSP operations automatically get correct position mapping

### PluginRuntime (`Plugin/PluginRuntime.cpp`)

Executes a single Luau plugin in a sandboxed environment:

- Uses `luaL_sandbox()` for security
- Timeout enforcement via interrupt callback
- Plugin must return a table (`transformSource` is optional)

### PluginManager (`Plugin/PluginManager.cpp`)

Orchestrates multiple plugins:

- All plugins receive the original source (not chained)
- Combines edits from all plugins into a single list (overlap detection happens later in `SourceMapping` construction, not here)
- Logs plugin loading and errors via Client

### PluginLuaApi (`Plugin/PluginLuaApi.cpp`)

Registers the `lsp.*` Lua API surface available to plugins (filesystem, JSON, client logging, Uri userdata).

### PluginDefinitions (`Plugin/PluginDefinitions.cpp`)

Provides Luau type definitions for the plugin API so that plugins get autocomplete and type checking.

## Integration Points

- **WorkspaceFileResolver** (`src/WorkspaceFileResolver.cpp`): Calls `PluginManager::transform()` and wraps results in `PluginTextDocument`. Caches plugin documents and invalidates them on source changes. Also provides `isPluginFile()` / `getEnvironmentForModule()` which assigns the `"LSPPlugin"` environment to loaded plugin files, enabling type definitions from `PluginDefinitions` for autocomplete in plugin scripts.
- **ClientConfiguration** (`src/include/LSP/ClientConfiguration.hpp`): `ClientPluginConfiguration` holds `enabled`, `paths`, `timeoutMs`, and `fileSystem` settings.

## Position Indexing

The Lua plugin API uses **1-indexed** positions (line 1, column 1 = first character). `PluginRuntime::parsePosition()` converts to 0-indexed `Luau::Location` internally. The comment in `TextEdit.hpp` ("0-indexed") refers to the internal C++ representation, not the Lua API surface.

## Multiple Plugins

When multiple plugins are configured:

1. All plugins receive the **original** source
2. Each plugin returns edits against the original
3. All edits are combined into a single list
4. If any edits overlap, an error is logged and no transformation is applied
5. The combined edits produce the final transformed source

This parallel approach is simpler than chaining and ensures plugins don't need to be aware of each other.
