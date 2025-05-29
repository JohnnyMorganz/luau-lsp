# Change Log

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

### Changed

- For DM types, `.Parent` is now typed with a write type of "Instance" in the new solver, preventing false-positive type
  errors ([#1039](https://github.com/JohnnyMorganz/luau-lsp/issues/1039))

### Fixed

- Fixed require-by-string failing for files named `luau` (e.g, `project/luau.luau`)

## [1.48.0] - 2025-05-28

### Added

- Internal caught but unhandled exceptions are now reported to Sentry if crash reporting is enabled
- Added progress indicator for watched files changes
- A Cloudflare page is now available to serve the type definition files and API documentations, due to GitHub ratelimiting ([#1059](https://github.com/JohnnyMorganz/luau-lsp/issues/1059))

  - `https://luau-lsp.pages.dev/type-definitions/globalTypes.None.d.luau`
  - `https://luau-lsp.pages.dev/type-definitions/globalTypes.PluginSecurity.d.luau`
  - `https://luau-lsp.pages.dev/type-definitions/globalTypes.LocalUserSecurity.d.luau`
  - `https://luau-lsp.pages.dev/type-definitions/globalTypes.RobloxScriptSecurity.d.luau`
  - `https://luau-lsp.pages.dev/api-docs/en-us.json`

### Changed

- Sync to upstream Luau 0.675
- Significant performance improvements to initial workspace indexing time (~3m to ~20s on 30,000 files)
- ~1160x performance improvement to Uri parsing time at scale (7.77s to 0.0067s for ~30,000 URIs)
- ~10x performance improvement to require resolution for string requires and virtual (sourcemap) -> real file paths (
  3.28s to 0.322s for ~30,000 files)
- ~1.29x performance improvement to reading files (4.5s to 3.66s for ~30,000 files)

### Fixed

- Fixed blowup of `linux-arm64` release build size due to the inclusion of debug symbols
- Fixed crash when auto imports is enabled and the file contains a require with no arguments (e.g.
  `local Value = require()`, typically the case when in the middle of typing a require)
- Fixed performance regression from previous release where type checking is being performed on document change even if
  workspace diagnostics is disabled
- Fixed crash where catch-all error handler attempts to send an error message with non-UTF 8 characters
- Fixed find references not showing types used as return types for
  functions ([#1060](https://github.com/JohnnyMorganz/luau-lsp/issues/1060))
- VSCode: if fetching API documentation / definitions fails, then we do not overwrite the existing file ([#740](https://github.com/JohnnyMorganz/luau-lsp/issues/740))

## [1.47.0] - 2025-05-17

### Added

- Implemented opt-in crash reporting for the language server. This is enabled via `luau-lsp.server.crashReporting.enabled`. Note: crash reporting
  sends network requests to [Sentry](https://sentry.io/). Crash Reporting is only available on Windows and macOS, and is not enabled in Standalone mode (`luau-lsp analyze`).

### Changed

- Sync to upstream Luau 0.674

## [1.46.0] - 2025-05-13

### Added

- Added progress indicator during indexing of initial startup. Currently does not provide much detail, but does provide
  some feedback to indicate the server is doing work.
- VSCode: server binary location setting (`luau-lsp.server.path`) now supports relative paths ([#1036](https://github.com/JohnnyMorganz/luau-lsp/issues/1036))

### Changed

- Sync to upstream Luau 0.673
- Luau FFlags are now synced from the Studio channel rather than the main channel on VSCode, which is typically updated
  earlier ([[#1040](https://github.com/JohnnyMorganz/luau-lsp/issues/1040)])

### Fixed

- Fixed invalid variable names created for instance-based auto-imports
- Fixed diagnostics not getting updated for dependent files when a file is edited and workspace diagnostics is
  enabled ([#1042](https://github.com/JohnnyMorganz/luau-lsp/issues/1042))

## [1.45.0] - 2025-05-04

### Added

- Autocomplete in string requires now respects `luau-lsp.completion.imports.ignoreGlobs` for filtering out files to
  skip (e.g., one can add `*.server.luau` or
  `*.client.luau`) ([#1014](https://github.com/JohnnyMorganz/luau-lsp/issues/1014))
- End autocompletion now also autocompletes missing `then` or `do` tokens in control flow statements (e.g., pressing
  enter after typing just `if condition`) ([#999](https://github.com/JohnnyMorganz/luau-lsp/issues/999))

### Changed

- Sync to upstream Luau 0.672
- Color picker will now strip redundant decimal digits (i.e. `0.0000` ->
  `0`) ([#1022](https://github.com/JohnnyMorganz/luau-lsp/issues/1022))
- The language server now follows the [new require-by-string semantics for
  `init.luau` files](https://rfcs.luau.org/abstract-module-paths-and-init-dot-luau.html) for file resolution and string
  require autocomplete ([#1023](https://github.com/JohnnyMorganz/luau-lsp/issues/1023))

### Fixed

- Fixed string-require auto imports not using aliases correctly on Windows ([#1025](https://github.com/JohnnyMorganz/luau-lsp/issues/1025))
- Instance-based auto-imports now use expression syntax when parts are not a valid identifier (e.g.
  `require(Modules["react-spring"])`) ([#1026](https://github.com/JohnnyMorganz/luau-lsp/issues/1026))
- Auto-imports no longer create invalid variable names ([#1026](https://github.com/JohnnyMorganz/luau-lsp/issues/1026))

## [1.44.1] - 2025-04-24

### Fixed

- Fixed incorrect serialization of maps that are keyed by a Uri, causing commands that apply edits such as Rename to
  fail

## [1.44.0] - 2025-04-24

### Removed

- The deprecated `luau-lsp.require.fileAliases` and `luau-lsp.require.directoryAliases` no longer show up in
  autocomplete for string requires. Use aliases as part of `.luaurc` instead

### Added

- Auto-importing now supports string requires. In standard platform, string requires is the default when
  `luau-lsp.completion.imports.enabled` and `luau-lsp.completion.imports.suggestRequires` are enabled. For the Roblox
  platform, string require auto imports must be switched on using `luau-lsp.completion.imports.stringRequires.enabled` (
  default: `false`) ([#494](https://github.com/JohnnyMorganz/luau-lsp/issues/494)).

### Changed

- Reimplementation of string require autocomplete based off upstream Luau require work
- Updated implementation of Uris internally to handle case-insensitive file systems correctly

### Fixed

- Fixed diagnostics not updating on windows for Neovim or Zed due to mismatched URI
  cases ([#988](https://github.com/JohnnyMorganz/luau-lsp/issues/988))
- Fixed "isIgnoredFile failed: relative path is default-constructed" for Neovim Windows
  users ([#752](https://github.com/JohnnyMorganz/luau-lsp/issues/752))

## [1.43.0] - 2025-04-19

### Added

- Added setting `luau-lsp.server.communicationChannel` with options `stdio` (default) or `pipe` to support communicating
  using a UNIX socket file connection instead of stdin/stdout. On the server, this is configured by passing
  `--pipe=FILE` to the command line. This is useful for attaching a debugger to the server on macOS. `pipe` is not
  supported on Windows. ([#998](https://github.com/JohnnyMorganz/luau-lsp/issues/998))

### Changed

- Sync to upstream Luau 0.670
- Linux runners for GitHub actions are bumped from the deprecated `ubuntu-20.04` to `ubuntu-22.04`. This may mean that release artifacts no longer work on `ubuntu-20.04`.
- Improved Studio plugin error message when attempting to connect to a server but it is not online ([#982](https://github.com/JohnnyMorganz/luau-lsp/issues/982))
- VSCode extension will now strip the prefix `FFlag` / `DFFlag` / `FInt` / `DFInt` if they were included in the overrides name ([#981](https://github.com/JohnnyMorganz/luau-lsp/issues/981))

### Fixed

- Fixed a bug when the New Type Solver is enabled where diagnostics would not update in dependent files when a file was
  changed until the dependent file is modified

## [1.42.1] - 2025-04-06

### Changed

- Sync to upstream Luau 0.668

### Fixed

- Don't show function types with function kind or label details when autocompleting in a type
  context ([#987](https://github.com/JohnnyMorganz/luau-lsp/issues/987))
- Fixed crash when registering a non-Roblox definitions file that contains classes named `Object` / `Instance` /
  `ServiceProvider` / `EnumItem` ([#986](https://github.com/JohnnyMorganz/luau-lsp/issues/986))

## [1.42.0] - 2025-03-27

### Changed

- Sync to upstream Luau 0.666
- Optimized re-indexing on changed files when large number of changes processed at once
- Watched files re-indexing now respects ignore globs specification

### Fixed

- Added a debounce of 1s on file changes triggering sourcemap generation when `luau-lsp.sourcemap.useVSCodeWatcher` is enabled
- Fixed crashing due to bad memory access when hovering over an imported type reference due to documentation computation
- Fixed sources of crashing and flakiness when hovering + go to definition on imported type references in the new solver
- Reduced the need to run 2 type checks unnecessarily on files when the new type solver is enabled

## [1.41.0] - 2025-03-16

### Added

- Documentation comments now attach to type alias definitions ([#956](https://github.com/JohnnyMorganz/luau-lsp/pull/956))
- VSCode: Introduced `luau-lsp.sourcemap.generatorCommand` to run a custom generator for updating the sourcemap. Accepts a shell command.
  If undefined (default), then falls back to using `rojo`. ([#968](https://github.com/JohnnyMorganz/luau-lsp/issues/968))
- VSCode: Introduced `luau-lsp.sourcemap.useVSCodeWatcher` (default: `false`). When enabled, the extension will connect to
  VSCode file added / removed events for retriggering the generator. When disabled (default), the extension delegates to
  the generator process for watching. When using `rojo`, this option controls the `--watch` flag.
- VSCode: Documentation now supports HTML syntax ([#964](https://github.com/JohnnyMorganz/luau-lsp/pull/964))

### Changed

- Sync to upstream Luau 0.665
- VSCode: Improved error reporting when the Studio Plugin sends a result that is too large. The error now includes the
  size limit, the received size, and steps to resolve the issue ([#969](https://github.com/JohnnyMorganz/luau-lsp/issues/969))
- The language server will no longer attempt to remove HTML tags from
  documentation ([#964](https://github.com/JohnnyMorganz/luau-lsp/pull/964))

## [1.40.0] - 2025-03-01

### Added

- Added configuration `luau-lsp.types.disabledGlobals` to support removing globals from the main scope for analysis.
  Accepts a list of libraries or library methods (e.g., `table`, `string.split`,
  etc.) ([#888](https://github.com/JohnnyMorganz/luau-lsp/issues/888))

### Changed

- Sync to upstream Luau 0.663
- Improved error reporting when the studio plugin server in VSCode fails to start up (typically due to port already in use) ([#936](https://github.com/JohnnyMorganz/luau-lsp/issues/936))

### Fixed

- Fixed an issue where lint warnings would suddenly disappear when typing characters / saving a file, and only reappear
  after further edits
- Autocompleting a table property that matches a keyword will now autocomplete correctly with braces (i.e., `t.then` ->
  `t["then"]`) ([#937](https://github.com/JohnnyMorganz/luau-lsp/issues/937))

## [1.39.2] - 2025-02-15

### Changed

- Sync to upstream Luau 0.661

### Fixed

- Fixed unintended bump of minimum supported macOS version in 1.39.0 to macOS 14. The language server binary supports a minimum version of macOS 10.15
- Single-line documentation comments (`---`) now correctly preserve newlines ([#917](https://github.com/JohnnyMorganz/luau-lsp/pull/917)).
- Fixed stack-use-after-free crash in sourcemap definition magic functions when the new solver is
  enabled ([#928](https://github.com/JohnnyMorganz/luau-lsp/issues/928))

## [1.39.1] - 2025-02-08

### Fixed

- Fixed server failing to start up due to attempting to run an invalid path as the server binary

## [1.39.0] - 2025-02-08

### Added

- Added configuration `luau-lsp.server.path` (default: `""`) which allows the use of locally installed `luau-lsp` binaries. ([#897](https://github.com/JohnnyMorganz/luau-lsp/pull/897))
- Auto-import require path information is added in `CompletionItem.labelDetails.description` as well as just in
  `CompletionItem.detail`
- In VSCode, the opening brace `{` character will be automatically closed with a `}` character when completed within an interpolated string. This occurs when the `{` character is typed just before whitespace or a `` ` `` character ([#916](https://github.com/JohnnyMorganz/luau-lsp/issues/916)).
- Added support for Luau's fragment autocomplete system. This can be enabled by configuring `luau-lsp.completion.enableFragmentAutocomplete` (default: `false`). This incremental system can lead to performance improvements when autocompleting.
- The `luau-lsp.ignoreGlobs` and `luau-lsp.types.definitionFiles` configuration from a settings JSON file will now be
  applied when running `luau-lsp analyze --settings file.json` on the command
  line ([#892](https://github.com/JohnnyMorganz/luau-lsp/issues/892))

### Changed

- Sync to upstream Luau 0.660

### Fixed

- Fixed erroneous `unknown notificated method: $/plugin/full` message in logs even though plugin message was handled
- Linux ARM releases are now built on arm-based GitHub runners, and hence should support Linux ARM properly

## [1.38.1] - 2025-01-12

### Changed

- Sync to upstream Luau 0.656

## [1.38.0] - 2024-12-28

### Added

- Auto-import requires will now show the require path in the details section rather than just "Auto-import". This will
  help to disambiguate between multiple modules with the same name but at different
  paths. ([#593](https://github.com/JohnnyMorganz/luau-lsp/issues/593))
- Added configuration `luau-lsp.inlayHints.hideHintsForErrorTypes` (default: `false`) to configure whether inlay hints
  should be shown for types that resolve to an error type ([#711](https://github.com/JohnnyMorganz/luau-lsp/issues/711))
- Added configuration `luau-lsp.inlayHints.hideHintsForMatchingParameterNames` (default: `true`) to configure whether
  inlay hints are shown where a variable name matches the
  parameter ([#779](https://github.com/JohnnyMorganz/luau-lsp/issues/779))
- Find all references of a returned function / table in a module will now return all cross-module
  references ([#879](https://github.com/JohnnyMorganz/luau-lsp/issues/879))
- Go To Definition inside of a require call will now lead to the required file, similar to Document Link. This is useful
  for editors that do not support Document Link ([#612](https://github.com/JohnnyMorganz/luau-lsp/issues/612))
- The recursive parameter in `game:FindFirstChild("ClassName", true)` is now supported for DataModel awareness. We
  recursively find the closest descendant based on BFS ([#689](https://github.com/JohnnyMorganz/luau-lsp/issues/689))
- We now attach a semantic token modifier to usages of `self` in non-colon function definitions (i.e.
  `function T.foo(self, ...)`) if `self` is the first
  argument ([#456](https://github.com/JohnnyMorganz/luau-lsp/issues/456))

### Changed

- The server no longer computes `relatedDocuments` in a `textDocument/diagnostic` request unnecessarily if the client
  does not support related documents
- Go To Definition on a cross-module function reference will now only resolve to the function definition, rather than
  also including the require statement
  `local func = require(path.to.function)` ([#878](https://github.com/JohnnyMorganz/luau-lsp/issues/878)).
- Overhauled the globbing mechanism leading to significant performance improvements in indexing, workspace diagnostics,
  and auto suggest requires, removing globbing as a bottleneck. Some results in example codebases ranging from 190 to
  1900 KLoC:
  - Initial indexing: ~3.32x to ~7.1x speedup (43.60s -> 13.12s and 9.42s ->
    1.33s) ([#829](https://github.com/JohnnyMorganz/luau-lsp/issues/829))
  - Workspace diagnostics: ~2.39x to ~4.75x speedup (164.93s -> 69.07s and 8.49s ->
    1.79s)
  - Auto-suggest requires: ~4.77x speedup (1.15s ->
    0.24s) ([#749](https://github.com/JohnnyMorganz/luau-lsp/issues/749))
  - These improvements heavily depend on the amount of code you have matching ignore globs. Workspace diagnostics
    improvements depends on the performance of Luau typechecking.

### Fixed

- Fixed isIgnoredFile check failing due to mismatching case of drive letter on Windows ([#752](https://github.com/JohnnyMorganz/luau-lsp/issues/752))
- Fixed `luau-lsp analyze --ignore GLOB` not ignoring files matching the glob if the files are within one of the
  provided directories ([#788](https://github.com/JohnnyMorganz/luau-lsp/issues/788))
- Fixed production VSCode extension failing to find server binary when debugging another VSCode extension ([#644](https://github.com/JohnnyMorganz/luau-lsp/issues/644))
- The Studio Plugin settings configuration is now stored in TestService instead of AnalyticsService to allow it to persist across sessions ([#738](https://github.com/JohnnyMorganz/luau-lsp/issues/739))
- The Studio Plugin now correctly sends updates to the language server when an instance changes name or ancestry hierarchy ([#636](https://github.com/JohnnyMorganz/luau-lsp/issues/636))
- Fixed Studio Plugin leaking Instance connections after disconnecting from server

## [1.37.0] - 2024-12-14

### Added

- The VSCode extension now registers a JSON schema for `.luaurc` files, providing simple diagnostics and intellisense ([#850](https://github.com/JohnnyMorganz/luau-lsp/pull/850))

### Changed

- Sync to upstream Luau 0.655

### Fixed

- Fixed `luau-lsp analyze --settings=...` crashing when a malformed settings JSON file is provided. Now, it will print the json error and continue assuming the settings did not exist
- Fixed regression in require by string autocompletion failing to correctly autocomplete files under directories ([#851](https://github.com/JohnnyMorganz/luau-lsp/issues/851))
- Autocompletion in string requires will now show aliases in their original case defined in `.luaurc`, rather than all lowercased

## [1.36.0] - 2024-11-30

### Deprecated

- `luau-lsp.require.fileAliases` and `luau-lsp.require.directoryAliases` are now deprecated in favour of aliases in `.luaurc` files. These settings will be removed in a future version.

### Added

- Added a setting to enable the new solver without having to tinker with FFlags: `luau-lsp.fflags.enableNewSolver`.
- Support parsing and resolving aliases coming from `.luaurc` files in module resolution and autocomplete
- Added two configuration options `luau-lsp.diagnostics.pullOnChange` and `luau-lsp.diagnostics.pullOnSave` to configure when document diagnostics updates for a file (default: `true`)

### Changed

- `workspace/diagnostic` call is ~98% faster when `luau-lsp.diagnostics.workspace` is disabled on large projects ([#826](https://github.com/JohnnyMorganz/luau-lsp/issues/826))
- Sync to upstream Luau 0.653

## [1.35.0] - 2024-11-10

### Removed

- Removed `luau-lsp.require.mode` in preparation for Luau's new require by string semantics. The default is now relative to the file the require is from.

### Added

- Added bracket pairs colorization for `<>` for generic types
- Added configuration option `luau-lsp.sourcemap.sourcemapFile` to specify a different name to use for the sourcemap
- A function call on a table with a `__call` metamethod will now show Signature Help and documentation ([#724](https://github.com/JohnnyMorganz/luau-lsp/issues/724))
- We now warn about non-alphanumeric FFlag names, and trim any leading/trailing whitespace in FFlag configuration on the VSCode extension ([#648](https://github.com/JohnnyMorganz/luau-lsp/issues/648))

### Changed

- Sync to upstream Luau 0.650

### Fixed

- Fixed autocompletion, type registration, hover types/documentation, and some crashes for cases where the new solver is enabled
- Fixed the refinement for `typeof(inst) == "Instance"` since `Object` became the root class type ([#814](https://github.com/JohnnyMorganz/luau-lsp/issues/814))
- Fixed inlay hints incorrectly showing for first parameter in static function when the function is called as a method (with `:`) ([#766](https://github.com/JohnnyMorganz/luau-lsp/issues/766))
- Fixed bracket pair completion breaking inside of generic type parameter list ([#741](https://github.com/JohnnyMorganz/luau-lsp/issues/741))
- Don't show aliases after a directory separator is seen in require string autocompletion ([#748](https://github.com/JohnnyMorganz/luau-lsp/issues/748))
- Fixed crashing of overload resolution in signature help when new solver is enabled ([#823](https://github.com/JohnnyMorganz/luau-lsp/issues/823))

## [1.34.0] - 2024-10-27

### Changed

- Sync to upstream Luau 0.649

### Fixed

- Fixed internal handling of Roblox datatypes segfaulting due to introduction of "Object" class

## [1.33.1] - 2024-10-05

### Changed

- Sync to upstream Luau 0.646

### Fixed

- Fixed a regression in 1.30.0 breaking type definitions files that rely on mutations like `Enum.Foo`
  ([#658](https://github.com/JohnnyMorganz/luau-lsp/issues/658))

## [1.33.0] - 2024-09-27

### Changed

- Sync to upstream Luau 0.644
- The VSCode extension will now sync flags beginning with `FIntLuau`, `DFFlagLuau` and `DFIntLuau` (previously it would only sync `FFlagLuau`)

## [1.32.4] - 2024-09-11

### Changed

- Sync to upstream Luau 0.642

## [1.32.3] - 2024-08-10

### Fixed

- Fixed a regression in 1.32.2 breaking resolution of virtual paths from real paths, particularly around `script` and relative usages of it. ([#734](https://github.com/JohnnyMorganz/luau-lsp/issues/734), [#735](https://github.com/JohnnyMorganz/luau-lsp/issues/735))

## [1.32.2] - 2024-08-10

### Changed

- Sync to upstream Luau 0.638

### Fixed

- Fixed a regression in 1.32.0 causing `luau-lsp.ignoreGlobs` and `luau-lsp.completion.imports.ignoreGlobs` to not work ([#719](https://github.com/JohnnyMorganz/luau-lsp/issues/719))
- Fixed auto-imports injecting a require in the middle of a multi-line require when introducing a require with lower lexicographical ordering ([#725](https://github.com/JohnnyMorganz/luau-lsp/issues/725))
- Fixed documentation not showing for properties of an intersected type table in Hover and Autocomplete ([#715](https://github.com/JohnnyMorganz/luau-lsp/issues/715))

## [1.32.1] - 2024-07-23

### Changed

- Sync to upstream Luau 0.635

### Fixed

- Fixed `:FindFirstChild()` returning `Instance` instead of `Instance?` when applied to DataModel types
- Fixed `:FindFirstChild()` not supporting a boolean "recursive" 2nd parameter on DataModel types ([#704](https://github.com/JohnnyMorganz/luau-lsp/issues/704))
- Fixed `:WaitForChild()` not supporting a number "timeout" 2nd parameter on DataModel types ([#704](https://github.com/JohnnyMorganz/luau-lsp/issues/704))
- Fixed inlay hint off-by-one on a function definition with an explicit self (i.e., `function Class.foo(self, param)`) ([#702](https://github.com/JohnnyMorganz/luau-lsp/issues/702))
- Fixed `:GetPropertyChangedSignal()` still showing children in autocomplete for DataModel types ([#699](https://github.com/JohnnyMorganz/luau-lsp/issues/699))
- Fixed grandparent's children showing up in autocomplete of FindFirstChild
- Fixed tilde expansion (`~`) for paths to home directory not working in VSCode ([#707](https://github.com/JohnnyMorganz/luau-lsp/issues/707))

## [1.32.0] - 2024-07-14

### Added

- Support tilde expansion (`~`) to home directory for definition and documentation file paths
- Added a datamodel-aware `WaitForChild` function
- We now apply a datamodel-aware `FindFirstChild` function to the top level datamodel and service types ([#543](https://github.com/JohnnyMorganz/luau-lsp/issues/543))
- Added autocompletion of children to `:FindFirstChild("")` and `:WaitForChild("")` ([#685](https://github.com/JohnnyMorganz/luau-lsp/issues/685))
- Attached magic function to `Instance.fromExisting` to allow it to operate similar to `inst:Clone` ([#678](https://github.com/JohnnyMorganz/luau-lsp/issues/678))
- Added separate configuration `luau-lsp.completion.imports.ignoreGlobs` to filter out files for auto-importing. We no longer check `luau-lsp.ignoreGlobs`. ([#686](https://github.com/JohnnyMorganz/luau-lsp/issues/686))
- Diagnostics will now refresh when the Studio plugin sends updated DataModel information ([#637](https://github.com/JohnnyMorganz/luau-lsp/issues/637))

### Changed

- Sync to upstream Luau 0.634
- Reverted "configuration in initializationOptions" and reintroduced messages postponing due to crashes in clients that do not send initial config

### Fixed

- Fix static linking with MSVC Runtime for release binaries
- Fixed clients that do not support pull diagnostics erroring with "server not yet received configuration for diagnostics"
- Don't show children in autocomplete for `:GetPropertyChangedSignal("")` ([#684](https://github.com/JohnnyMorganz/luau-lsp/issues/684))
- Fixed autocomplete end not working for non-local functions ([#554](https://github.com/JohnnyMorganz/luau-lsp/issues/554))
- Fixed extension failing to get types information on macOS with "'fetch' is not defined"
- Fixed crashes under new type solver due to internal removal of different type inference for autocomplete/non-autocomplete contexts ([#692](https://github.com/JohnnyMorganz/luau-lsp/issues/692))
- Fixed crashes where internally we fail to normalise a "virtual path" from a sourcemap to a real path, causing it to mismatch with the filepath understood by VSCode, leading to desync in internal file state. ([#645](https://github.com/JohnnyMorganz/luau-lsp/issues/645))

## [1.31.1] - 2024-07-07

### Fixed

- The binary on Windows now statically links to the MSVC Runtime to make it more portable ([#657](https://github.com/JohnnyMorganz/luau-lsp/issues/657))
- Fixed Roblox types still showing when setting `luau-lsp.platform.type` to something other than `roblox`. ([#668](https://github.com/JohnnyMorganz/luau-lsp/issues/668))

## [1.31.0] - 2024-07-01

### Changed

- Sync to upstream Luau 0.632
- Language clients are recommended to send configuration during intializationOptions (see <https://github.com/JohnnyMorganz/luau-lsp/blob/main/editors/README.md> for details)
- Removed need for postponing requests whilst waiting for platform configuration (relies on clients sending config in intializationOptions)

### Fixed

- Fixed crashes occuring for users without the MSVC Redistributable installed due to introduced dependency on Windows headers ([#657](https://github.com/JohnnyMorganz/luau-lsp/issues/657))

## [1.30.1] - 2024-06-27

### Fixed

- Fixed use-after-free when generating sourcemap types

## [1.30.0] - 2024-06-23

### Deprecated

- Deprecated `luau-lsp.types.roblox` setting in favour of `luau-lsp.platform.type`

### Added

- Added `luau-lsp.platform.type` to separate platform-specific functionality from the main LSP
- Added option `--platform` to analyze CLI to make configuring `luau-lsp.platform.type` more convenient
- Added support for registering FFlags for the server via initializationOptions, rather than on the command line ([#590](https://github.com/JohnnyMorganz/luau-lsp/issues/590))
- Added `luau-lsp.inlayHints.makeInsertable` (default: `true`) to configure whether inlay hint type annotations can be inserted by clicking ([#620](https://github.com/JohnnyMorganz/luau-lsp/issues/620))
- Added inlay hints for varargs parameter type ([#622](https://github.com/JohnnyMorganz/luau-lsp/issues/622))
- Added setting `luau-lsp.plugin.maximumRequestBodySize` (default: `3mb`) to configure the maximum size of the payload accepted from the Studio Plugin
- Added support for requiring `.toml` files
- Added syntax highlighting for `luau` in markdown fenced codeblocks

### Changed

- Sync to upstream Luau 0.631
- An indexed expression will no longer show an inlay hint if the index matches the parameter name (i.e., `call(other.value)` won't add `value:` inlay hint) ([#618](https://github.com/JohnnyMorganz/luau-lsp/issues/618))
- Studio Plugin will now perform Gzip compression on sent requests

### Fixed

- Overloaded methods (typed as an intersection of function types with explicitly defined `self`) are now correctly marked with `method` semantic token ([#574](https://github.com/JohnnyMorganz/luau-lsp/issues/574))
- Fixed semantic token highlighting overrides for global variables
- Improved robustness for non-ASCII filesystem paths in file lookup and directory traversal

## [1.29.1] - 2024-05-19

### Changed

- Sync to upstream Luau 0.626

### Fixed

- Type aliases now show generics in the type hover ([#591](https://github.com/JohnnyMorganz/luau-lsp/issues/591))
- Fixed 'find all references' not working for a global function declared in a file
- Likewise, rename now supports global functions defined in a file ([#568](https://github.com/JohnnyMorganz/luau-lsp/issues/568))

## [1.29.0] - 2024-05-11

### Added

- Bytecode display will now show type info information. Added setting `luau-lsp.bytecode.typeInfoLevel` (default: 1) to configure the [type info level](https://github.com/luau-lang/luau/blob/259e50903855d1b8be79edc40fc275fd04c9c892/Compiler/include/Luau/Compiler.h#L29-L33) shown.
- Added "magic functions / refinements" support under the New Solver (i.e., special handling of :IsA, :FindFirstChildWhichIsA, :Clone, etc.)

### Changed

- Sync to upstream Luau 0.625
- Improved memory usage of document and workspace diagnostics by no longer storing type graphs
- Rewritten the Luau grammar syntax: <https://github.com/JohnnyMorganz/Luau.tmLanguage>

### Fixed

- Fixed autocompletion of strings with '/' characters causing the prefix to be duplicated rather than replaced ([#607](https://github.com/JohnnyMorganz/luau-lsp/issues/607))
- Fixed bug with string requires where a required files types may not correctly update when the file contents changed

## [1.28.1] - 2024-03-04

### Fixed

- Fixed macos release build

## [1.28.0] - 2024-03-03

### Changed

- Sync to upstream Luau 0.615
- Non-function properties will now no longer be shown by default when autocompleting a method call (e.g., `foo:bar`).
  To revert back to the original behaviour, enable `luau-lsp.completion.showPropertiesOnMethodCall`
- Support Ubuntu 20.04

### Fixed

- Autocompletion of variables that hold a class type will now correctly have a kind of "variable" rather than "class"
- Introduced a fix for orphaned `rojo` processes after VSCode has closed
- `FindFirstAncestor` method now correctly finds the project root in non-DataModel projects
- Fixed bad handling of unicode in filesystem paths causing crashes on server startup
- Gracefully handle filesystem errors when visiting directories for indexing / workspace diagnostics

## [1.27.1] - 2024-01-20

### Changed

- Sync to upstream Luau 0.609

### Fixed

- Switched to memory-efficient implementation of workspace diagnostics (currently behind FFlag `LuauStacklessTypeClone3`)
- Improved handling of configuration info received from non-VSCode clients
- Functions with explicitly defined `self` parameters are correctly marked with the `method` semantic token

## [1.27.0] - 2023-12-25

### Added

- Autocompletion items for items marked as `@deprecated` via documentation comments will now reflect their deprecated status
- Show string literal byte length and utf8 characters on hover
- Support passing `--settings` to `luau-lsp lsp` configuring the default global settings to use
- Added support for viewing textual bytecode and compiler remarks using commands `Luau: Compute Bytecode for file` and `Luau: Compute Compiler Remarks for file`.
  This opens up a new view with bytecode/remarks inlined as comments in the source file
  - Added configuration `luau-lsp.bytecode.vectorLib`, `luau-lsp.bytecode.vectorCtor` and `luau-lsp.bytecode.vectorType` to configure compiler options when generating bytecode
  - Custom editors should handle the `luau-lsp/bytecode` and `luau-lsp/compilerRemarks` LSP message to integrate compiler remarks info in their editor
- Added `luau-lsp.types.robloxSecurityLevel` to select what security level to use for the API types, out of: `None`, `LocalUserSecurity`, `PluginSecurity` and `RobloxScriptSecurity`

### Changed

- Sync to upstream Luau 0.607
- Made rename operation fully backed by find all references, to ensure both return results that are consistent with each other
- Hide return type hints for no-op functions
- Changed the VSCode registered language and grammar ID from `lua` to `luau`. **NOTE:
  ** this may affect existing custom themes!
- Renamed `script/globalTypes.d.lua` to `script/globalTypes.d.luau` (the old file will be kept temporarily for compatibility)
- Default security level of API types changed from `RobloxScriptSecurity` to `PluginSecurity` - set `luau-lsp.types.robloxSecurityLevel` to `RobloxScriptSecurity` to see original behaviour
- Improved warning message when Rojo not found when attempting to generate sourcemap, with option to configure settings to disable autogeneration

### Fixed

- Fixed Find All References / Rename not working on a table property defined inline, such as `name` in:

```lua
local T = {
  name = "string"
}
```

- Fixed methods and events showing up in "GetPropertyChangedSignal" autocomplete
- Fixed requiring a directory containing "init.lua" not working
- Fixed go to definition on a property of a table that stores a cross-module type value (e.g. the result of a function defined in another module)

## [1.26.0] - 2023-11-19

### Added

- Added support for documentation comments on table type properties:

```lua
type Foo = {
  --- A documentation comment
  map: () -> ()
}
```

- We now show the file path in the completion description when auto-requiring files

### Changed

- Sync to upstream Luau 0.604
- Overhauled command line argument parsing system to be more consistent and flexible
- Deprioritized `loadstring` in autocomplete
- `luau-lsp.diagnostics.strictDatamodelTypes` now defaults to `false` on the language server side (note, it was already default `false` in VSCode).
  Defaulting to `true` was unintentional. This will affect external language client users (e.g. neovim)
- Analyze CLI tool now respects `luau-lsp.diagnostics.strictDatamodelTypes` if set in the provided configuration.
  The flag `--no-strict-dm-types` still remains for backwards compatibility reasons, but is now deprecated.

### Fixed

- Attempting to rename a generic type parameter now correctly renames it in all locations
- Fixed renaming a local variable not appropriately renaming any imported types
- Auto-import requires will now show the full codeblock that will be inserted, rather than just the first line if also inserting a service

## [1.25.0] - 2023-10-14

### Changed

- Sync to upstream Luau 0.599
- Prioritise `game:GetService()` as the first autocompletion entry when typing `game:`
- Code blocks in hover and documentation now use `luau` as the syntax highlighting

### Fixed

- Do not add line separator in hover when there is no text documentation
- Fixed init files not working with directory aliases (e.g. `require("@dir")` or `require("@dir/subdir")`)

## [1.24.1] - 2023-09-09

### Changed

- Sync to upstream Luau 0.594
- Support autocomplete end on `do` blocks

### Fixed

- Fixed crash when attempting to Go To Definition of an imported type

## [1.24.0] - 2023-08-26

### Changed

- Sync to upstream Luau 0.592
- Simplified Instance.new and game:GetService calls internally and in the definitions file to reduce complexity issues in the typechecker.

### Fixed

- Fixed cleanup of rojo sourcemap generation process when VSCode exits
- Fixed color presentations values being unclamped causing errors in other editors
- Fixed newline not added to separate services and requires when the suggestion imports both at the same time

## [1.23.0] - 2023-08-06

### Added

- Added command `luau-lsp.reloadServer` to restart the language server without having to reload the workspace

### Changed

- Sync to upstream Luau 0.589
- Changes to settings which require server restart will now reload the server instead of having to reload the whole VSCode workspace
- Switch to Rojo `rojo sourcemap --watch` command for sourcemap autogeneration. Note that on rojo error, you must manually restart sourcemap regeneration.
  **Requires Rojo v7.3.0+**

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
- Added support for moonwave-style documentation comments! Currently only supports comments attached to functions directly. See <https://eryn.io/moonwave> for how to write doc comments
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
- Fixed bug where `_:` would not be removed as the name of function arguments. `function foo(_: number, _: number)` will now show as `function foo(number, number)`
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
- `_:` will no longer show up in hover/signature help for unnamed function parameters

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
