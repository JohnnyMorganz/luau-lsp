# Change Log

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

### Changed

- Sync to upstream Luau 0.590

## [1.23.0] - 2023-08-06

### Added

- Added command `luau-lsp.reloadServer` to restart the language server without having to reload the workspace

### Changed

- Sync to upstream Luau 0.589
- Changes to settings which require server restart will now reload the server instead of having to reload the whole VSCode workspace
- Switch to Rojo `rojo sourcemap --watch` command for sourcemap autogeneration. Note that on rojo error, you must manually restart sourcemap regeneration. **Requires Rojo v7.3.0+**

### Fixed

- Reverted change to type checking in 1.22.0 that reduced memory footprint. This should resolve the problems where diagnostics aren't showing with an InternalCompilerError, at the cost of increased memory use if `luau-lsp.diagnostics.workspace` is enabled.
- Fixed string require resolution when the string had a secondary extension: `Module.mod` will be resolved as `Module.mod.luau`
- Fixed resolution of directory aliases pointing to relative paths

## [1.22.1] - 2023-07-15

### Changed

- Sync to upstream Luau 0.584
- Removed need for typechecking for operations that don't require the type information (e.g., document link / color)

### Fixed

- Fixed diagnostics not showing when working in a new file with no workspace open
- Fixed race condition where sometimes the server does not receive user configuration on initial load, falling back to defaults
- `luau-lsp.fflags.override` will now be parsed when using CLI analyze settings. Note that the other `fflags` options are not supported in CLI analyze mode
- Fixed semantic tokens segfault crash on some tables
- Fixed duplicate definitions showing in the Go To Definition page
- Fixed some syntax highlighting inconsistencies
- Added a temporary fix to "RecursionLimitException" exceptions leaking to the public interface.

## [1.22.0] - 2023-06-30

### Added

- Support requiring directories with `init.luau` (or `init.lua`) files inside of them. i.e. `require("../Directory")` resolves to `../Directory/init.luau`.
- The CLI analyze now accepts a parameter `--settings=path/to/settings.json` which takes in LSP-style settings to configure features such as require settings. Note: this is separate to `.luaurc`

#### Changed

- Significant improvements to memory usage in large workspaces when workspace indexing or diagnostics are enabled
- Sync to upstream Luau 0.582
- Deprioritise file or directory aliases over exact paths in autocomplete, since typically aliases start with a prefix (e.g. `@`)
- Signature Help is more intelligent about providing information about the best function overload which matches

### Fixed

- Fixed language features not working in new untitled files
- Fixed incorrect color conversions in the color picker between RGB/HSV/Hex
- Fixed autoimporting modules not respecting multiline requires
- Fixed documentation for `debug` and `utf8` library
- Fixed synthetic `typeof()` showing up in signature help for builtin tables (e.g. `function typeof(string).byte(...)`)
- Fixed signature help highlighting for `string` library
- Fixed rename symbol on a type definition `type NAME = ...`
- Fixed file and directory aliases not being canonicalised to absolute paths causing "Follow Link" to fail when using relative alias paths
- Don't show directory aliases after the first path segment has been typed
- Fixed rename symbol not working when triggered at the end of a symbol
- Fix indentation of autocomplete end when autocompleting inside of a function call

## [1.21.0] - 2023-06-14

### Deprecated

- Deprecated `luau-lsp.autocompleteEnd` setting in favour of `luau-lsp.completion.autocompleteEnd`

### Added

- Added folding ranges for multi-line function definitions, so long parameter lists can be collapsed
- Added notification when we detect definitions file changes
- Added support for auto-requiring modules when autocompleteing a type reference, to allow indexed types: i.e. autocompleting `Module` in `type Foo = Module.Bar`
- Added `luau-lsp.require.directoryAliases` to map require string _prefixes_ to directories
- Added tilde expansion to `luau-lsp.require.fileAliases` (and `directoryAliases`), mapping `~/foo.lua` to a file in your home directory
- End autocompletion will now work for defined anonymous functions inside of function calls

### Changed

- Sync to upstream Luau 0.580
- Updated workspace indexing strategy to minimise memory usage. We no longer index ignored files (`luau-lsp.ignoreGlobs`),
  and there is a setting `luau-lsp.index.maxFiles` (default: 10,000) to configure the amount of files indexed before backing off.

### Fixed

- When editing in model projects, we now force relative requires, instead of incorrect absolute requires using a "ProjectRoot"

## [1.20.2] - 2023-05-10

### Fixed

- Fixed internal error for LazyType unwrapping

## [1.20.1] - 2023-05-09

### Fixed

- Fixed bug in semantic tokens system causing language server crashes

## [1.20.0] - 2023-05-09

### Added

- Added support for workspace symbols to resolve symbols across the whole workspace. In VSCode, you can open this using `Ctrl + T`
- Added configuration option `luau-lsp.require.fileAliases` to statically provide custom mappings from string requires to a file path.
  For example, adding `@example/constants` mapping to `C:/fakepath/constants.luau` will automatically resolve `require("@example/constants")`
- Added support for Folding Ranges. The language server now signals the following foldable ranges in a document:
  - Whole blocks, such as `do .. end`, `for - do .. end` `function() .. end` etc.
  - Tables, and type tables `x = { .. }`
  - Multiline function calls `foo(..)`
  - Block comments `--[[ .. ]]`
  - Custom comment regions denoted using `--#region` and `--#endregion`
- Added support for Call Hierarchies. Call Hierarchies allow you to view all incoming and outgoing calls of a function:
  i.e., all functions that call the current function, as well as all functions that the current function calls.
  This works at multiple levels, displaying ancestor and descendant functions.

### Changed

- Sync to upstream Luau 0.575

## [1.19.2]

### Fixed

- Fixed forced expressive types in diagnostics
- Added option `--no-strict-dm-types` for analyze CLI to disable strict datamodel types and its associated false positives

## [1.19.1] - 2023-04-27

### Fixed

- Fixed regression in DataModel item autocompletion

## [1.19.0] - 2023-04-26

### Deprecated

- Deprecated config `luau-lsp.completion.suggestImports`: use `luau-lsp.completion.imports.enabled` instead

### Added

- Added setting `luau-lsp.index.enabled` which will index the whole workspace into memory. If disabled, only limited support for Find All References and rename is possible
- Added support for finding all references of both local and exported types. For exported types, `luau-lsp.index.enabled` must be enabled for full support.
- Added support for renaming table properties across files. If `luau-lsp.index.enabled` is disabled, this feature is disabled for correctness reasons.
- Added support for renaming types (both local and exported). If `luau-lsp.index.enabled` is disabled, this exported types renaming is disabled for correctness reasons.
- Added more settings to auto-importing:
  - `luau-lsp.completion.imports.enabled`: replaces `luau-lsp.completion.suggestImports` (default: false)
  - `luau-lsp.completion.imports.suggestServices`: whether GetService imports are included in suggestions (default: true)
  - `luau-lsp.completion.imports.suggestRequires`: whether auto-requires are included in suggestions (default: true)
  - `luau-lsp.completion.imports.requireStyle`: the style of require format (default: "auto")
  - `luau-lsp.completion.imports.separateGroupsWithLine`: whether an empty line should be added in between services and requires (default: false)

### Changed

- Sync to upstream Luau 0.573
- Improved find all references system for tables. We can now track all references to table and its properties across files. This requires `luau-lsp.index.enabled` to be enabled for full support.

### Fixed

- Fixed pull diagnostics result not following spec
- Fixed errors when file has shebang `#!` present at top of file
- Fixed string require autocompletion failing when autocomplete triggered on an incomplete string, e.g. `require("Constants/Te|")`.
  Originally, nothing would autocomplete. Now, everything inside of the Constants folder will still autocomplete as usual (filtered for "Te").

## [1.18.1] - 2023-03-23

### Fixed

- Fixed server crash when auto require imports is enabled and there is a type-asserted require present in the file (`require(location) :: any`)
- Fixed additional automatic service imports when completing an automatic require import being placed before a hot comment (such as `--!strict`)
- Fixed automatic require import being placed incorrectly we also autocomplete a service. This can be shown when there is a multiline comment, and the service is imported above that comment, but the require gets imported inside of the comment incorrectly.

## [1.18.0] - 2023-03-20

### Added

- Added support for changing `Color3` colors using the color picker
- Added support for automatic require imports (currently only for Roblox mode). If you start typing the name of a module in your code, you can autocomplete the require statement automatically. This feature is enabled by setting `luau-lsp.completion.suggestImports`.

### Changed

- Sync to upstream Luau 0.568.
  In particular, this provide improvements to control flow analysis refinements. This allows the type checker to recognise type
  options that are unreachable after a conditional/unconditional code block. e.g.:

```lua
local function x(x: string?)
    if not x then return end

    -- x is 'string' here
end
```

To enable this feature, the FFlag `LuauTinyControlFlowAnalysis` must currently be enabled.

- The language server will only be enabled on "file" and "untitled" schemes. This means it will be disabled in diff mode
  and live share. This is because we cannot yet provide sufficient information in these contexts.

## [1.17.1] - 2023-03-04

### Changed

- Sync to upstream Luau 0.566

### Fixed

- Don't autocomplete another set of parentheses on a function call if they already exist
- Fix `.luaurc` in current working directory not taken into account when calling `luau-lsp analyze`

## [1.17.0] - 2023-02-12

### Added

- Added two code actions: `Sort requires` and `Sort services` (services only enabled if `luau-lsp.types.roblox` == true).
  These actions will sort their respective groups alphabetically based on a variable name set.
  You can also set these actions to automatically run on save by configuring:

```json
"editor.codeActionsOnSave": {
    "source.organizeImports": true
}
```

### Changed

- Sync to upstream Luau 0.563
- Prioritised common services and Instance properties/methods in autocomplete so that they show up first

### Fixed

- Further fixes to document symbols failing due to malformed ranges

## [1.16.4] - 2023-02-10

### Fixed

- Fixed document symbols crashing due to internal malformed data

## [1.16.3] - 2023-02-09

### Fixed

- Changed internal representation of documents to reduce the likelihood of Request Failed for "No managed text document"

## [1.16.2] - 2023-02-01

### Fixed

- Fixed document symbol crash on incomplete functions
- Fixed `--base-luaurc` not registering for an LSP server
- Fixed crashing on invalid FFlags configuration - the VSCode client will now validate the flags

## [1.16.1] - 2023-01-30

### Fixed

- Fixed error in document symbols not conforming to specification - `selectionRange` will now be fully enclosed by `range`

## [1.16.0] - 2023-01-29

### Added

- Support documentation comments attached to a table and table types, e.g. on `DATA` in the followign:

```lua
--- Doc comment
local DATA = {
    ...
}

--- Doc comment
type Contents = {
	...
}
```

- Include documentation comments on functions and tables in autocompletion
- Added configuration option `luau-lsp.require.mode` to configure how string requires are resolved. It can either be `relativeToWorkspaceRoot` (default) or `relativeToFile`
- Added `luau-lsp.types.documentationFiles` to support adding extra documentation symbols to the database. These are used to support definition files, and should be in the same format as [shown here](https://raw.githubusercontent.com/MaximumADHD/Roblox-Client-Tracker/roblox/api-docs/en-us.json)
- Added `luau-lsp.diagnostics.strictDatamodelTypes` (default: `false`) which configures whether we use expressive DataModel types to power diagnostics.
  When off, `game` / `script` / `workspace` (and all their members) are typed as `any`, which helps to prevent false positives, but may lead to false negatives.
- Added CLI option `--base-luaurc=PATH` for both LSP and Analyze mode to provide a path to a `.luaurc` file which is used as the default configuration
- Added support for Go To Definition / Type Definition on imported type references `module.Type` (gated behind FFlag `SupportTypeAliasGoToDeclaration`)

### Changed

- Sync to upstream Luau 0.560
- Class symbols with no documentation present in the docs file will no longer show anything on hover/autocomplete (i.e. won't show `@luau/global/require`)
- `Instance.new()` now excepts variables which are of type string without erroring. It will instead error when Instance.new is called with a string literal which is an unknown class name
- In the CLI, `luau-lsp lsp` now supports passing multiple `--docs=` parameters
- The CLI will now error when an unknown option is passed to it
- Diagnostics will now be emitted on `.luaurc` files with parse errors

### Fixed

- Fixed unknown require errors occurring in multi-root workspaces when in a folder which isn't the first one
- Fixed diagnostics not clearing for files which were deleted unconventionally (i.e., outside of VSCode using File Explorer, or external commands such as `git stash`)

## [1.15.0] - 2023-01-11

### Added

- Added syntax highlighting support for interpolated strings
- Added color viewers for Color3.new/fromRGB/fromHSV/fromHex
- Added support for autocompleting require-by-string functions with files and folders
- Support documentation comments on variables:

```lua
--- documentation comment
local x = "string"

--- another doc comment
local y = function()
end
```

- Support documentation comments on table properties, such as the following:

```lua
local tbl = {
    --- This is some special information
    data = "hello",
    --- This is a doc comment
    values = function()
    end,
}

local x = tbl.values -- Should give "This is a doc comment"
local y = tbl.data -- Should give "This is some special information"
```

### Changed

- Sync to upstream Luau 0.558
- All Luau FFlags are no longer enabled by default. This can be re-enabled by configuring `luau-lsp.fflags.enableByDefault`. It is recommended to keep `luau-lsp.fflags.sync` enabled so that FFlags sync with upstream Luau
- Allow variable number of `=` sign for multiline doc comments, so `--[[` and `--[===[` etc. are valid openers

### Fixed

- Luau analyze now exits with code 0 if there are no reported errors (all errors are ignored)
- `require(instance:FindFirstChild("Testing", true))` will no longer resolve as an immediate child of instance due to the recursive argument
- Fixed a bug where internally the wrong pointer to an Instance type was being used for DM nodes which manifested into failed unification and `never` types
- Constant variables will now be syntax highlighted appropriately at definition site (`local CONSTANT`)

## [1.14.3] - 2022-12-10

### Changed

- Sync to upstream Luau 0.556 (fixes crashing problems)
- Sync to latest language client

## [1.14.2] - 2022-12-04

### Changed

- Sync to upstream Luau 0.555 (in particular, this has improvements to class definitions)

## [1.14.1] - 2022-11-23

### Changed

- Sync to upstream Luau 0.554

### Fixed

- Fixed stack overflow when looking up properties on a table type where the `__index` is set to itself

## [1.14.0] - 2022-11-13

### Added

- Show inlay hints for variables in `for x in y` loops

### Changed

- Sync to upstream Luau 0.553

### Fixed

- Fixed rename symbol not working when cursor after variable
- Fixed rename symbol causing server crashing when attempting to rename a token which is not a variable

## [1.13.1] - 2022-10-29

### Fixed

- Respect client capabilities for snippet support in completion items
- Respect `luau-lsp.completion.addParentheses` when `fillCallArguments` is enabled
- Fixed Inlay Hints crash when calling a function which only takes varargs
- Fixed Request Failed due to "No managed text document" as URLs were not being updated correctly

## [1.13.0] - 2022-10-28

### Added

- Show documentation for overloaded functions in completion and hover. We show the documentation string of the first overload, and how many other overloads are present.
- Show documentation for builtin class methods in signature help, including for the correct overload
- Show documentation for parameters in signature help
- Added `luau-lsp.completion.addParentheses` and `luau-lsp.completion.addTabstopAfterParentheses` to configure whether parentheses are added when completing a function call, and whether we include a tab stop after the parentheses respectively.
- Automatically fill function call arguments using parameter names. This can be disabled using `luau-lsp.completion.fillCallArguments`.

### Changed

- Sync to upstream Luau 0.551
- Hide parameter name and variable inlay hint if the name is just `_`

### Fixed

- Fixed string-based requires to use a fully-qualified file path, fixing Document Link (Follow Link) support for requires
- Fixed reverse dependencies not being marked as dirty when using string requries due to unnormalised file paths
- Fixed incorrect highlighting of unnamed parameters in signature help when multiple parameters present of same type
- Fixed documentation not provided for some built-ins on hover
- Fixed signature help highlighting of parameters named `_`
- Fixed documentation comments of parent function being attached to a nested function
- Use location to determine which parameter is active in signature help
- Correctly handle highlighting variadic arguments in signature help
- [Sublime Text] Fixed push diagnostics not being recomputed when sourcemap or `.luaurc` changes

## [1.12.1] - 2022-10-18

### Fixed

- Fixed attempting to run workspace diagnostics on null workspace causing Internal Server errors (affecting Sublime Text)

## [1.12.0] - 2022-10-18

### Added

- Provide autocomplete for class names in `Instance:IsA("ClassName")` and errors when ClassName is unknown
- Provide autocomplete for properties in `Instance:GetPropertyChangedSignal("Property")` and errors when Property is unknown
- Provide autocomplete for enums in `EnumItem:IsA("enum")` and errors when Enum is unknown
- Added support for moonwave-style documentation comments! Currently only supports comments attached to functions directly. See https://eryn.io/moonwave for how to write doc comments
- Added command line flag `--ignore=GLOB` to `luau-lsp analyze` allowing you to provide glob patterns to ignore diagnostics, similar to `luau-lsp.ignoreGlobs`. Repeat the flag multiple times for multiple patterns

### Changed

- Sync to upstream Luau 0.549
- Deprioritise metamethods (`__index` etc.) in autocomplete

### Fixed

- Fixed inlay hints not showing for variable types when `hover.strictDataModelTypes` is disabled
- Fixed Internal Errors for workspace diagnostics when a type error was being displayed backed by the incorrect text document causing string errors
- Fixed Internal Errors for goto definitions as incorrect document used for string conversions
- Fixed overloaded functions not being highlighted as functions in autocomplete
- Potential fix to Request Failed errors
- Fixed `self` incorrectly showing up in Inlay Hints and Signature Help
- Fixed Studio Plugin syncing causing server crashes

## [1.11.2] - 2022-10-08

### Changed

- Sync to upstream Luau 0.548

### Fixed

- Fixed inlay hints no longer showing up
- Fixed inlay hints not showing up in first load of file until a dummy change is made
- Fixed DM types not generated for `script` nodes. Improved autocomplete will now be provided for non-DataModel projects (e.g. `Tool` as Root)

## [1.11.1] - 2022-10-01

### Changed

- Sync to upstream Luau 0.547

### Fixed

- Fixed handling of UTF-16 characters of different size to UTF-8 (i.e., emojis, non-english text). Will no longer produce malformed strings and weird diagnostics

## [1.11.0] - 2022-09-28

### Added

- Added support for Semantic Tokens

### Changed

- Improved autocomplete items ordering by applying heuristics to sort items
- Table keys are prioritised when autocompleting inside of a table

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
