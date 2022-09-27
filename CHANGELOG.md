# Change Log

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

### Added

- Added support for Semantic Tokens

### Changed

- Improved autocomplete items ordering by applying heuristics to sort items

### Fixed

- Fixed `.meta.json` file being picked as a script's file path instead of the actual Luau file
- Fixed diagnostics not clearing for files when workspace diagnostics is not enabled
- Fixed metatable name not being used when hovering over the function of a metatable
- Manually increased some internal limits to reduce likelihood of type errors
- Fixed diagnostics (and other global configuration) not loading when not inside of a workspace
- Fixed server erroring when configuration is not sent by the client
- Fixed diagnostics not showing on initial startup in push diagnostics mode (Sublime Text)
- Fixed "insert inlay hint" incorrectly enabled for error types

## [1.10.1] - 2022-09-24

### Changed

- Further improvements to instance type creation
- Sync to upstream Luau 0.546

### Fixed

- Children of `game` will now correctly show in autocomplete
- Fix autocomplete of non-identifier properties: `Packages._Index.roblox_roact-rodux@0.2.1` -> `Packages._Index["roblox_roact-rodux@0.2.1"]`
- Fixed mapping of requires from `game.Players.LocalPlayer.PlayerScripts` to `game.StarterPlayer.StarterPlayerScripts` (and PlayerGui + StarterGear)
- Fixed type errors being reported twice in `luau-lsp analyze`

## [1.10.0] - 2022-09-17

### Added

- Introduced a Studio plugin to infer instance trees for partially managed projects. This works alongside Rojo sourcemaps, where instance information retrieved from Studio is merged into the sourcemap. Starting the plugin can be configured using `luau-lsp.plugin.enabled`. Install the plugin from the [Plugin Marketplace](https://www.roblox.com/library/10913122509/Luau-Language-Server-Companion)

### Changed

- Sync to upstream Luau 0.545
- Inlay hints for variables will no longer show if the type hint string is the same as the variable name (i.e., `local tbl = {}`, the hint `: tbl` will no longer show) ([#137](https://github.com/JohnnyMorganz/luau-lsp/issues/137))
- Restructured instance types system to reduce memory and type creation footprint

### Fixed

- Fixed false document diagnostics showing up for opened tabs when VSCode is first started ([#132](https://github.com/JohnnyMorganz/luau-lsp/issues/132))

## [1.9.2] - 2022-09-06

### Changed

- Sync to upstream Luau 0.543

### Fixed

- Fixed diagnostics for ignored files not clearing when workspace diagnostics is enabled ([#77](https://github.com/JohnnyMorganz/luau-lsp/issues/77))
- Fixed `luau-lsp analyze` would not exit with non-zero error code when definitions failed to load
- Fixed `luau-lsp analyze` would not exit with non-zero error code when file path provided was not found
- Fixed crash when Suggest Imports is enabled and you have a local variable defined with no assigned value (e.g. `local name`)

## [1.9.1] - 2022-08-29

### Changed

- Sync to upstream Luau 0.542

### Fixed

- New service imports which come first alphabetically will group with existing imports rather than going at the beginning of the file
- Fixed warning messages showing up as notifications when generating Rojo Sourcemap even if it works successfully

## [1.9.0] - 2022-08-16

### Added

- Added configuration options to enable certain Language Server features. By default, they are all enabled:

  - `luau-lsp.completion.enabled`: Autocomplete
  - `luau-lsp.hover.enabled`: Hover
  - `luau-lsp.signatureHelp.enabled`: Signature Help

- Added configuration option `luau-lsp.hover.showTableKinds` (default: off) to indicate whether kinds (`{+ ... +}`, `{| ... |}`) are shown in hover information
- Added configuration option `luau-lsp.hover.multilineFunctionDefinitions` (default: off) to spread function definitions in hover panel across multiple lines
- Added configuration option `luau-lsp.hover.strictDatamodelTypes` (default: on) to use strict DataModel type information in hover panel (equivalent to autocomplete). When disabled, the same type information that the diagnostic type checker uses is displayed
- Added support for automatic service importing. When using a service which has not yet been defined, it will be added (alphabetically) to the top of the file. Config setting: `luau-lsp.completion.suggestImports`

### Changed

- Sync to upstream Luau 0.540

### Fixed

- The types of `:FindFirstChild`, `:FindFirstAncestor` and `:FindFirstDescendant` have been changed to return `Instance?`
- `:GetActor` is fixed to return `Actor?`
- Fixed bug when using `--definitions=` when calling `luau-lsp analyze`

## [1.8.1] - 2022-08-01

### Fixed

- Fixed `self` being showed as the first inlay hint incorrectly in parameter names and types

## [1.8.0] - 2022-07-30

### Added

- Added support for cross-file go to definition of functions
- Added support for go-to definition of properties defined on a metatable with `__index`
- Added support for inlay hints. It can be enabled by configuring `luau-lsp.inlayHints.parameterNames`, `luau-lsp.inlayHints.parameterTypes`, `luau-lsp.inlayHints.variableTypes`, `luau-lsp.inlayHints.functionReturnTypes`.

### Changed

- Sync to upstream Luau 0.538
- Improved completion detail function param information with better representative types, and include a trailing type pack if present

### Fixed

- Fixed crash when hovering over local in incomplete syntax tree
- Fixed language server not working for newly created files not yet stored on disk
- Luau LSP will now activate if you run an LSP command
- Fixed finding the incorrect workspace folder to analyze with in a multi-workspace environment

## [1.7.1] - 2022-07-17

### Fixed

- Fix crash when hovering over function type definitions

## [1.7.0] - 2022-07-16

### Added

- Reintroduced support for workspace diagnostics, with proper streaming support. Enable `luau-lsp.diagnostics.workspace` for project wide diagnostics.
- You can now hover over a type node to get type information. In particular, this works for properties inside type tables, and hovering over `typeof()`, allowing you to determine what typeof resolved to.
- Added Go To Definition for type references
- Added `Luau: Regenerate Rojo Sourcemap` command to force regeneration of a Rojo sourcemap
- Improved case where project file `default.project.json` was not found. We search for other project files, and prompt a user to configure

### Changed

- Sync to upstream Luau 0.536
- Improved extension error message when Rojo version present does not have sourcemap support
- Document links will now resolve for requires in all locations of a file, not just the top level block

### Fixed

- A diagnostic refresh will now be requested once sourcemap contents change
- With the introduction of workspace diagnostics, ignored files should now only show diagnostics when specifically opened
- Document symbols for method definitions now correctly use a colon instead of a dot operator.
- Fixed crash when hovering over a type node
- Fixed go to definition on a global just going to the top of the file. It will now not accept go to definition requests
- Fixed using absolute file paths to point to definition files not working on Windows

## [1.6.0] - 2022-07-02

### Added

- Added `luau-lsp.sourcemap.enabled` option which dictates whether sourcemap-related features are enabled
- Diagnostics will now be provided for definitions files which errored
- Added `luau-lsp.sourcemap.rojoPath` to explicitly specify the path to a Rojo executable instead of relying on it being available from the workspace directory
- Added hover information when hovering over a type definition

### Changed

- Moved definitions file loading to post-initialization
- Sync to upstream Luau 0.534
- A `_` will no longer show when we can't determine a function name / a function isn't named.

### Fixed

- Fixed regression where diagnostics are not cleared when you close an ignored file
- Fixed errors sometimes occuring when you index `script`/`workspace`/`game` for children
- Fixed internal error caused by `:Clone()` calls when called on an expression which isn't an Lvalue (e.g., `inst:FindFirstChild(name):Clone()`)
- Fixed bug where `_: ` would not be removed as the name of function arguments. `function foo(_: number, _: number)` will now show as `function foo(number, number)`
- Fixed analyze mode not exiting with a non-zero exit code when there are errors
- Fixed excessive whitespace in document symbols for expr-named function defintions
- Fixed hover for global functions and local variables

## [1.5.2] - 2022-06-22

### Changed

- Downgraded missing types/documentation files to a warning in the logs instead of a window error. It is common for no file to be provided in a vanilla build.

### Fixed

- Fixed diagnostics not showing for any file after the first one you open

## [1.5.1] - 2022-06-21

### Fixed

- Module name will now be included in hover and signature displays for imported types
- Fixed bug where in-memory contents of a document which was deleted were not cleared, causing spurious errors when recreating a new document of the same name.
- Fixed duplicate diagnostics displayed and never clearing when `workspace` diagnostics configuration was enabled

## [1.5.0] - 2022-06-20

### Added

- Added predicate logic to `EnumItem:IsA("Type")` so it will now narrow the type of an EnumItem when used as a predicate
- Added setting to configure whether all Luau FFlags are enabled by default. This can be configured using `luau-lsp.fflags.enableByDefault` or `--no-flags-enabled` command line option. Currently, all FFlags are enabled by default, but you can manually sync/override them.
- Added support for adding extra definition files to load using `luau-lsp.types.definitionFiles`
- Roblox definitions can now be disabled using `luau-lsp.types.roblox`
- Added label details to completion items

### Changed

- Sync to upstream Luau 0.532
- Updated to improve table type stringification, with an appropriate newline placed in between braces. This should result in better readable table types when hovering
- Upgraded vscode language client to 8.0, with support for LSP Specification 3.17
- VSCode Client now makes use of diagnostics pull model

### Fixed

- Fixed equality comparison between enum items raising a type error
- Fixed autocompletion of properties with spaces not correctly being converted into a `["property"]` index leading to a type error
- Fixed function stringification when using an expression index call such as `data["property"]()`
- Fixed workspace diagnostics not respecting ignore globs for dependent files

## [1.4.0] - 2022-06-12

### Added

- Added Document Symbols support
- Merged [luau-analyze-rojo](https://github.com/JohnnyMorganz/luau-analyze-rojo) into the project to simplify maintenance. To run a standalone analysis tool, run `luau-lsp analyze`
- Added links to Luau Documentation for lint warnings
- Added documentation to Enum types
- Diagnostics of dependents (i.e., files which require a changed file) will now refresh to display type errors when the file changes
- Added support for workspace-wide diagnostics to report in all files. You can enable this using `luau-lsp.diagnostics.workspace`. It is disabled by default

### Changed

- Sync to upstream Luau 0.531
- Unused lints will are now tagged appropriately so they render faded out

### Fixed

- Fixed the equality comparison between two Instance types causing a type error
- Fixed false positive type errors occuring when using DataModel instance types. Unfortunately, for this fix we had to temporarily type all DataModel instances as `any`. You should still get proper autocomplete and intellisense, however type errors will no longer throw for unknown children.
- Fixed enum types to be under the "Enum" library, so the types are referenced using `Enum.Font` instead of `EnumFont`
- HTML Tags have been stripped from documentation so they should render better in IntelliSense
- Fixed "Text Document not loaded locally" error occuring when you start typing in a newly created file (as the sourcemap is not yet up-to-date)

## [1.3.0] - 2022-06-10

### Added

- Added support for configuring enabled Luau FFlags
- Added support for pulling in the currently enabled FFlags in Studio, in order to replicate behaviour

### Changed

- The "Updating API" message will now only show on the status bar instead of a popup notification.
- Instance types will now be named by the Class Name rather than the Instance name
- File system watchers for `.luaurc` and `sourcemap.json` are now registered on the server side

### Fixed

- Fixed requiring modules when using `:FindFirstAncestor("Ancestor")`
- Fixed requiring modules in `LocalPlayer.PlayerGui` / `LocalPlayer.PlayerScripts` / `LocalPlayer.StarterGear`
- Changing `.luaurc` configuration will now refresh the config cache and update internally

## [1.2.0] - 2022-06-04

### Added

- Player will now have `PlayerGui`, `Backpack`, `StarterGear` and `PlayerScripts` children, with the relevant Starter instances copied into it (StarterGui, StarterPack, PlayerScripts)
- `Instance:FindFirstChild("name")` and `Instance:FindFirstAncestor("name")` will now correctly resolve to the relevant instance type if found. This allows the type checker to correctly resolve children/parents etc.

### Changed

- Sync to upstream Luau 0.530

### Fixed

- Fixed extension repeatedly downloading latest API information when it is already up to date
- Fixed `self: Type` showing up in hover information/autocomplete when it is unnecessary at it has been inferred by the `:` operator
- Fixed extension not displaying error if calling out to `Rojo` command fails
- Fixed reverse dependencies not updating when types of required modules change (causing the type system to be incorrect). i.e., if you required script B in script A, and change script B, now the change will propagate to script A

## [1.1.0] - 2022-05-20

### Added

- Can disable automatic sourcemap generation in extension settings.
- Can change the project file used to generate sourcemap in extension settings (defaults to `default.project.json`).
- Can toggle whether non-script instances are included in the generated sourcemap (included by default).
- Added support for "Find References"
  - Currently only works for finding all references of a local variable in the current document. Cross-file references will come in future.
- Added support for "Rename"
  - Currently only works for local variables in the current document. Cross-file references will come in future.

### Changed

- Sync to upstream Luau 0.528

## [1.0.0] - 2022-05-19

### Added

- Added hover information for type references
- Added end autocompletion functionality, as done in Studio. Can be enabled through `luau-lsp.autocompleteEnd`
- Added automatic sourcemap regeneration when files change. This relies on `rojo` being available to execute in the workspace folder (i.e., on the PATH), with `rojo sourcemap` command support

### Changed

- Improved Go To Type Definition support
- Improved overall Go To Definition support
  - Can now handle function definitions in tables
  - Can handle cross-file definitions
  - Can handle deeply nested tables - multiple properties (incl. cross file support)
- Hovering over a property inside a table will now give you type information about the assigned expression, rather than just "string"

### Fixed

- Fixed syntax highlighting of generic type packs in function definitions

## [0.4.1] - 2022-05-17

### Fixed

- Fixed signature help not showing up
- Fixed markdown in completion not working

## [0.4.0] - 2022-05-17

### Added

- Added basic go to definition and go to type definition support
- Added support for pull-based diagnostics
- Added the base support for user configuration through extension settings
- Added support for marking specific globs as ignored (through extension settings). If a file is ignored, diagnostics will ONLY be displayed when the file is explicitly opened. If the file is closed, diagnostics will be discarded.
- Cross compiled macOS binary to arm64

### Changed

- Hover over definitions will now try to give more expressive types
- `self` will now no longer show up in hover/signature help if it has already been implicitly provided
- `_: ` will no longer show up in hover/signature help for unnamed function parameters

### Fixed

- Fixed hover over method function definitions not working
- Added a fallback to the prefix of a method name if we can't find the actual name
- Fixed diagnostics lost on a reopened file because we did not mark it as dirty
- Fixed invalid path computed for related diagnostics so they were not showing in the editor

## [0.3.0] - 2022-05-15

### Added

- Implemented Document Link Provider
- Added JSON require support
- Added Documentation support

### Changed

- Improved hover design
- `globalTypes.d.lua` and the API docs will now be automatically downloaded by the client and passed to the server. The user no longer needs to manage this.

### Fixed

- Fixed spurious diagnostics on initial load of a file

## [0.2.0] - 2022-05-14

### Added

- Implement Signature Help Provider
- Use incremental text document sync
- Enabled all Luau FFlags by defaults

### Changed

- Improved stringification of functions to look nicer in hover and signature help

## [0.1.2] - 2022-05-14

### Fixed

- Fix crash when workspace does not have a `sourcemap.json` present in root

## [0.1.1] - 2022-05-14

### Fixed

- Bug Fixes

## [0.1.0] - 2022-05-13

- Initial basic release
