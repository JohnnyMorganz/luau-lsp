# Plugin System

The plugin system allows you to write Luau scripts that transform source code before type checking. This enables custom syntax extensions, macro expansions, or other source transformations while maintaining correct position mapping for LSP features (diagnostics, hover, go-to-definition, etc.).

## Getting Started

Here's a complete walkthrough to get a plugin running:

**1. Create a plugin file** at `plugins/my_plugin.luau` in your workspace:

```luau
-- Replace all occurrences of "TODO" with "DONE"
return {
    transformSource = function(source, context)
        local edits = {}
        local line = 1
        local lineStart = 1

        for i = 1, #source do
            if string.sub(source, i, i) == "\n" then
                line += 1
                lineStart = i + 1
            end

            if string.sub(source, i, i + 3) == "TODO" then
                local col = i - lineStart + 1
                table.insert(edits, {
                    startLine = line,
                    startColumn = col,
                    endLine = line,
                    endColumn = col + 4,
                    newText = "DONE",
                })
            end
        end
        return edits
    end,
} :: PluginApi
```

**2. Add VS Code settings** in `.vscode/settings.json`:

```json
{
  "luau-lsp.plugins.enabled": true,
  "luau-lsp.plugins.paths": ["./plugins/my_plugin.luau"]
}
```

**3. Open any `.luau` file** — the plugin will transform all `TODO` identifiers to `DONE` for type checking purposes. You'll see your original source in the editor, but Luau will see the transformed version.

Plugins hot-reload automatically when you save changes to the plugin file — no need to restart the language server.

### Minimal Plugin

```luau
return {
    transformSource = function(source, context)
        -- Return nil or {} for no changes
        -- Return a list of TextEdits to transform the source
        return nil
    end
} :: PluginApi
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

| Setting                               | Type       | Default | Description                                               |
| ------------------------------------- | ---------- | ------- | --------------------------------------------------------- |
| `luau-lsp.plugins.enabled`            | `boolean`  | `false` | Enable source code transformation plugins                 |
| `luau-lsp.plugins.paths`              | `string[]` | `[]`    | Paths to Luau plugin scripts (relative to workspace root) |
| `luau-lsp.plugins.timeoutMs`          | `number`   | `5000`  | Timeout in milliseconds for plugin execution              |
| `luau-lsp.plugins.fileSystem.enabled` | `boolean`  | `false` | Allow plugins to read files within the workspace          |

## Plugin API

A plugin is a Luau script that returns a table with a `transformSource` function. Annotate the return value with `:: PluginApi` to get type checking and autocomplete for your plugin:

```luau
return {
    transformSource = function(source: string, context: PluginContext): {TextEdit}?
        -- Return nil or {} for no changes
        -- Return list of edits to transform the source
    end
} :: PluginApi
```

### Types

```luau
type TextEdit = {
    startLine: number,      -- 1-indexed
    startColumn: number,    -- 1-indexed, UTF-8 byte offset
    endLine: number,        -- 1-indexed
    endColumn: number,      -- 1-indexed, UTF-8 byte offset
    newText: string,
}

type PluginContext = {
    filePath: string,
    moduleName: string,
}
```

### Important: 1-Indexed Positions

Positions use **1-based** line and column numbers (not 0-based like LSP). The first character of a file is at line 1, column 1. Columns are measured in UTF-8 byte offsets.

### Sandbox Environment

Plugins run in a sandboxed Luau environment. Standard libraries (`string`, `table`, `math`, `bit32`, `buffer`, `coroutine`, `utf8`) are available. Direct filesystem and OS access (`io`, `os`, `debug`) is not available; use the `lsp.fs` API instead.

Plugin files that are listed in `plugins.paths` are automatically recognized by the language server and receive type information for the `lsp.*` API, giving you autocomplete and type checking while editing your plugins.

### How It Works

- Your plugin receives the **original** source code and returns a list of text edits
- The language server applies these edits to produce transformed source code
- Luau type-checks the transformed source
- LSP features (diagnostics, hover, etc.) automatically map positions back to your original source

### Multiple Plugins

When multiple plugins are configured, all plugins receive the **original** source (they are not chained). All edits from all plugins are combined. If any edits overlap, an error is logged and no transformation is applied.

## Example: Replace DEBUG with false

```luau
-- Replace all occurrences of "DEBUG" with "false"
return {
    transformSource = function(source, context)
        local edits = {}
        local line = 1
        local lineStart = 1 -- byte position where current line begins

        for i = 1, #source do
            if string.sub(source, i, i) == "\n" then
                line = line + 1
                lineStart = i + 1
            end

            if string.sub(source, i, i + 4) == "DEBUG" then
                local col = i - lineStart + 1
                table.insert(edits, {
                    startLine = line,
                    startColumn = col,
                    endLine = line,
                    endColumn = col + 5,
                    newText = "false",
                })
            end
        end
        return edits
    end
}
```

## Filesystem API

Plugins can optionally read files within the workspace through a sandboxed filesystem API. This must be explicitly enabled via `luau-lsp.plugins.fileSystem.enabled`.

### `lsp.workspace.getRootUri(): Uri`

Returns the workspace root as a Uri object.

### `lsp.fs.readFile(uri: Uri): string`

Reads a file within the workspace. Only files within the workspace can be read; attempting to read files outside the workspace throws an "access denied" error.

```luau
local ok, content = pcall(function()
    local rootUri = lsp.workspace.getRootUri()
    local fileUri = rootUri:joinPath("src", "config.luau")
    return lsp.fs.readFile(fileUri)
end)
if ok then
    -- Use content
else
    print("Failed:", content)
end
```

### `lsp.fs.exists(uri: Uri): boolean`

Checks whether a file exists within the workspace. Returns `true` if the file exists and is readable, `false` otherwise. Like `readFile`, only files within the workspace can be checked.

### `lsp.Uri.parse(uriString: string): Uri`

Parses a URI string into a Uri object.

### `lsp.Uri.file(fsPath: string): Uri`

Creates a `file://` Uri from a filesystem path.

### Uri Object

**Properties** (read-only): `scheme`, `authority`, `path`, `query`, `fragment`, `fsPath`

**Methods**:

- `:joinPath(...segments: string): Uri` - Join path segments, returns new Uri
- `:toString(): string` - Convert to URI string

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

### `lsp.client.sendLogMessage(type: string, message: string)`

Sends a log message to the editor. `type` can be `"error"`, `"warning"`, `"info"`, or `"log"`.

The global `print` function sends a log message at `info` level.

```luau
lsp.client.sendLogMessage("info", "Processing file...")
lsp.client.sendLogMessage("warning", "Deprecated syntax detected")
```

## JSON API

### `lsp.json.deserialize(jsonString: string): any`

Parses a JSON string and returns the corresponding Lua value.

```luau
local data = lsp.json.deserialize('{"name": "test", "count": 42}')
print(data.name)   -- "test"
print(data.count)  -- 42
```

## Error Handling

All API functions that can fail (filesystem, JSON) throw on error. Use `pcall`:

```luau
local ok, result = pcall(lsp.fs.readFile, uri)
if not ok then
    -- result contains error message
end
```

Common errors:

- `"filesystem access not available"` - `fileSystem.enabled` is `false`
- `"only file:// URIs are supported"` - Non-file URI passed
- `"access denied: file is outside workspace"` - Security violation
- `"file not found or cannot be read"` - I/O error
- `"JSON parse error: ..."` - Invalid JSON string

## Debugging

### View Internal Source

The language server provides a debug command to view the transformed source code that Luau actually type-checks. This is useful for verifying that your plugin produces the expected output.

In VS Code, run the command **"Luau Debug: View Internal Source Representation of File"** (`luau-lsp.debug.viewInternalSource`) while a `.luau` file is open. This opens a side-by-side view showing the transformed source, which updates live as you edit.

For other editors, send a `luau-lsp/debug/viewInternalSource` request with a `TextDocumentIdentifier` parameter. The response is the transformed source as a string.
