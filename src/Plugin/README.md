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

## Filesystem API

Plugins can optionally access files within the workspace through a sandboxed filesystem API.

### Enabling Filesystem Access

Filesystem access is disabled by default. To enable it:

```json
{
  "luau-lsp.plugins.fileSystem.enabled": true
}
```

### API Reference

#### `lsp.workspace.getRootUri(): Uri`

Returns the workspace root as a Uri object.

#### `lsp.fs.readFile(uri: Uri): string`

Reads a file within the workspace. Throws an error on failure.

**Security**: Only files within the workspace can be read. Attempting to read files outside the workspace will throw an "access denied" error.

```luau
local ok, content = pcall(function()
    local rootUri = lsp.workspace.getRootUri()
    local fileUri = rootUri:joinPath("src", "config.luau")
    return lsp.fs.readFile(fileUri)
end)
if ok then
    -- Use content
else
    warn("Failed:", content)
end
```

#### `lsp.Uri.parse(uriString: string): Uri`

Parses a URI string into a Uri object.

```luau
local uri = lsp.Uri.parse("file:///path/to/file.luau")
```

#### `lsp.Uri.file(fsPath: string): Uri`

Creates a file:// Uri from a filesystem path.

```luau
local uri = lsp.Uri.file("/path/to/file.luau")
```

### Uri Object

Uri is a userdata object with the following properties and methods:

**Properties** (read-only):

- `scheme: string` - URI scheme (e.g., "file")
- `authority: string` - URI authority
- `path: string` - URI path
- `query: string` - URI query string
- `fragment: string` - URI fragment
- `fsPath: string` - Platform-specific filesystem path

**Methods**:

- `:joinPath(...segments: string): Uri` - Join path segments, returns new Uri
- `:toString(): string` - Convert to URI string

### Error Handling

All filesystem operations throw on error. Use `pcall` to handle errors:

```luau
local ok, result = pcall(lsp.fs.readFile, uri)
if not ok then
    -- result contains error message
end
```

Possible errors:

- `"filesystem access not available"` - Setting is disabled
- `"only file:// URIs are supported"` - Non-file URI
- `"access denied: file is outside workspace"` - Security violation
- `"file not found or cannot be read"` - I/O error

### Example: Reading Configuration

```luau
return {
    transformSource = function(source, context)
        -- Try to read a configuration file
        local ok, configContent = pcall(function()
            local rootUri = lsp.workspace.getRootUri()
            local configUri = rootUri:joinPath(".luau-plugin-config.json")
            return lsp.fs.readFile(configUri)
        end)

        if not ok then
            -- Config file doesn't exist or can't be read, use defaults
            return nil
        end

        -- Use configuration to decide how to transform
        -- ...
        return nil
    end
}
```

## Client API

Plugins can send log messages to the LSP client for debugging and status reporting.

### `lsp.client.sendLogMessage(type: string, message: string)`

Sends a log message to the client.

**Parameters:**

- `type: string` - Message type: `"error"`, `"warning"`, `"info"`, or `"log"`
- `message: string` - The message content

```luau
lsp.client.sendLogMessage("info", "Processing file...")
lsp.client.sendLogMessage("warning", "Deprecated syntax detected")
lsp.client.sendLogMessage("error", "Failed to parse configuration")
```

The global `print` function will send a log message at `info` level.
