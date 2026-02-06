## Overture LSP

This is a fork of Luau LSP with changes specifically for the ungms workflow.

Changes from upstream Luau LSP:

* Added `Overture:LoadLibrary` support for resolving library types
* Added autocomplete in `Overture:LoadLibrary` calls
* Added `Overture:Get` support for resolving specified instance types
* Added autocomplete in `Overture:Get` calls
* Added support for `NamedImports` in `Overture:LoadLibrary` calls
* Added autocomplete for `NamedImports` in `Overture:LoadLibrary` calls
* Disabled `ImportUnused` lint warnings for the `Overture` variable
