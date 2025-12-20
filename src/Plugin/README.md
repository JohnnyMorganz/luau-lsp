# Plugin System

The plugin system allows users to write Luau scripts that transform source code before type checking. This enables custom syntax extensions, macro expansions, or other source transformations while maintaining correct position mapping for LSP features.

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

### TextEdit
Simple data structure representing a text edit with a range and replacement text. Uses Luau's `Location` type for the range.

### SourceMapping
Core position mapping between original and transformed source:
- Built from a list of `TextEdit`s
- Provides bidirectional position conversion
- Validates edits don't overlap (throws on overlap)
- Sorts edits by position before applying

### PluginTextDocument
Inherits from `TextDocument` and overrides key methods:
- `getText()` - returns transformed content
- `convertPosition()` - maps positions through SourceMapping
- LSP operations automatically get correct position mapping

### PluginRuntime
Executes a single Luau plugin in a sandboxed environment:
- Uses `luaL_sandbox()` for security
- Timeout enforcement via interrupt callback
- Plugin must return a table (transformSource is optional)

### PluginManager
Orchestrates multiple plugins:
- All plugins receive the original source (not chained)
- Combines edits from all plugins
- Rejects overlapping edits with an error
- Logs plugin loading and errors via Client

## Plugin API

Plugins are Luau scripts that return a table:

```luau
return {
    -- Optional: transform source code
    transformSource = function(source: string, context: PluginContext): {TextEdit}?
        -- Return nil or {} for no changes
        -- Return list of edits to transform the source
        -- Edits must not overlap (including with other plugins)
    end
}
```

### Types

```luau
type Position = {
    line: number,    -- 0-indexed
    column: number,  -- 0-indexed, UTF-8 byte offset
}

type Range = {
    start: Position,
    ["end"]: Position,
}

type TextEdit = {
    range: Range,
    newText: string,
}

type PluginContext = {
    filePath: string,
    moduleName: string,
    languageId: string,  -- "luau"
}
```

### Example Plugin

```luau
-- Replace all occurrences of "DEBUG" with "false"
return {
    transformSource = function(source, context)
        local edits = {}
        local pos = 1
        while true do
            local start, finish = string.find(source, "DEBUG", pos, true)
            if not start then break end

            -- Convert to 0-indexed line/column
            local line, col = 0, start - 1
            for i = 1, start - 1 do
                if string.sub(source, i, i) == "\n" then
                    line = line + 1
                    col = start - i - 1
                end
            end

            table.insert(edits, {
                range = {
                    start = { line = line, column = col },
                    ["end"] = { line = line, column = col + 5 }
                },
                newText = "false"
            })
            pos = finish + 1
        end
        return edits
    end
}
```

## Configuration

Plugins are configured via LSP client settings:

```json
{
    "luau-lsp.plugins.enabled": true,
    "luau-lsp.plugins.paths": ["./plugins/my_plugin.luau"],
    "luau-lsp.plugins.timeoutMs": 5000
}
```

## Multiple Plugins

When multiple plugins are configured:
1. All plugins receive the **original** source
2. Each plugin returns edits against the original
3. All edits are combined into a single list
4. If any edits overlap, an error is logged and no transformation is applied
5. The combined edits produce the final transformed source

This parallel approach is simpler than chaining and ensures plugins don't need to be aware of each other.
