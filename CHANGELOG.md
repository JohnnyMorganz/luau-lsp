# Change Log

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

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
