# Change Log

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

### Added

- Added hover information for type references
- Added end autocompletion functionality, as done in Studio. Can be enabled through `luau-lsp.autocompleteEnd`

### Changed

- Improved Go To Type Definition support
- Improved overall Go To Definition support
  - Can now handle function definitions in tables
  - Can handle cross-file definitions
  - Can handle deeply nested tables - multiple properties (incl. cross file support)
- Hovering over a property inside a table will now give you type information about the assigned expression, rather than just "string"

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
