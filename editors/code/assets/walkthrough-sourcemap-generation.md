# Sourcemap Generation

A sourcemap file is used to map paths on your file system to paths in the Roblox DataModel, and vice-versa.

The Luau Language Server will look for a `sourcemap.json` file in the root of your workspace.

## Generation Options

By default, the extension will attempt to run the following command in your working directory:

```
rojo sourcemap --watch default.project.json --output sourcemap.json
```

There are a couple options you can configure:

- `luau-lsp.sourcemap.rojoProjectFile`: what Rojo project file to use (default: `default.project.json`)
- `luau-lsp.sourcemap.includeNonScripts`: whether to include non-script instances in the sourcemap. If you have a very large project, you may wish to disable this setting (default: on)
- `luau-lsp.sourcemap.sourcemapFile`: what sourcemap file to use (default: `sourcemap.json`)

If you do not use Rojo to manage your project, you can customise the command run using `luau-lsp.sourcemap.generatorCommand`.
This will spawn a new process on startup. By default, we assume the generator command will watch files and regenerate the sourcemap as necessary.
If your generator does not support file watching, you can enable `luau-lsp.sourcemap.useVSCodeWatcher` which will execute the generator command each time VSCode detects a file change.

If you want to disable built-in sourcemap generation completely, and instead manage it yourself, you can disable the `luau-lsp.sourcemap.autogenerate` setting.

## Sourcemap Structure

If you need to generate your own sourcemap, it should follow the following structure:

```json
{
  "name": "Game",
  "className": "DataModel",
  "children": [
    {
      "name": "ReplicatedStorage",
      "className": "ReplicatedStorage",
      "children": [
        {
          "name": "Library",
          "className": "ModuleScript",
          "filePaths": ["ReplicatedStorage/Library.luau"]
        },
        {
          "name": "Logging",
          "className": "ModuleScript",
          "filePaths": ["ReplicatedStorage/Logging.luau"]
        }
      ]
    },
    {
      "name": "ServerScriptService",
      "className": "ServerScriptService",
      "children": [
        ...
      ]
    }
    ...
  ]
}
```
