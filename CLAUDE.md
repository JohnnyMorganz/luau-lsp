# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Luau Language Server (luau-lsp) is an implementation of the Language Server Protocol (LSP) for the Luau programming language. It provides IDE features like diagnostics, autocompletion, hover, go-to-definition, etc. The project also includes a standalone CLI (`luau-lsp analyze`) for CI type-checking and linting.

## Build Commands

```bash
# Initial clone (submodules required)
git clone https://github.com/JohnnyMorganz/luau-lsp.git --recurse-submodules

# Update submodules
git submodule update --init --recursive

# Configure build (from repo root)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug  # Use Debug for faster builds during development

# Build the CLI (use -j for parallel builds)
cmake --build . --target Luau.LanguageServer.CLI --config Debug -j$(nproc)

# Build tests (use Debug for faster iteration)
cmake --build . --target Luau.LanguageServer.Test --config Debug -j$(nproc)

# For release/production builds, use Release mode:
# cmake .. -DCMAKE_BUILD_TYPE=Release
# cmake --build . --target Luau.LanguageServer.CLI --config Release -j$(nproc)

# Build with ASAN (Linux/macOS)
cmake .. -DLSP_BUILD_ASAN:BOOL=ON
cmake --build . --target Luau.LanguageServer.Test -j$(nproc)
```

## Running Tests

Tests use the doctest framework. **Important:** Run tests from the repository root directory, as tests read from `tests/testdata/` using relative paths.

```bash
# Run all tests (from repo root)
./build/Luau.LanguageServer.Test

# Run with new type solver
./build/Luau.LanguageServer.Test --new-solver

# Run with all FFlags enabled
./build/Luau.LanguageServer.Test --fflags=true

# Run specific test by name
./build/Luau.LanguageServer.Test --test-case="TestName"

# List all tests
./build/Luau.LanguageServer.Test --list-test-cases
```

## Architecture

### Core Components

- **LanguageServer** (`src/LanguageServer.cpp`, `src/include/LSP/LanguageServer.hpp`): Main LSP message dispatcher. Handles JSON-RPC requests/notifications and routes to appropriate handlers.

- **WorkspaceFolder** (`src/Workspace.cpp`, `src/include/LSP/Workspace.hpp`): Represents a workspace folder. Contains the Luau `Frontend` for type checking. Implements all LSP operations (completion, hover, diagnostics, etc.).

- **WorkspaceFileResolver** (`src/WorkspaceFileResolver.cpp`): Implements Luau's `FileResolver` interface. Handles file reading, module resolution, and configuration loading.

- **LSPPlatform** (`src/include/Platform/LSPPlatform.hpp`): Base class for platform-specific behavior. Factory method `getPlatform()` returns either the base implementation or `RobloxPlatform`.

- **RobloxPlatform** (`src/include/Platform/RobloxPlatform.hpp`): Extends `LSPPlatform` with Roblox-specific features - sourcemap parsing, DataModel types, service auto-imports, Color3/BrickColor handling.

### LSP Operations

Located in `src/operations/`:

- Each file implements a specific LSP feature (Completion, Hover, GotoDefinition, References, Rename, etc.)
- Operations are methods on `WorkspaceFolder` that take LSP params and return LSP results

### Transport Layer

`src/transport/`: Handles JSON-RPC communication

- `StdioTransport`: Standard input/output for primary usage
- `PipeTransport`: Named pipe for alternative IDE integration

### Protocol Types

`src/include/Protocol/`: LSP protocol structures with nlohmann/json serialization

### External Dependencies

Located in `extern/` and `luau/`:

- `luau/`: Luau compiler and type checker (submodule)
- `extern/json/`: nlohmann/json for JSON handling
- `extern/glob/`: Glob pattern matching
- `extern/argparse/`: CLI argument parsing
- `extern/toml/`: TOML parsing
- `extern/doctest/`: Test framework

## Code Style

- C++17 standard
- Uses Allman brace style (configured in `.clang-format`)
- 4-space indentation, no tabs
- 150 column limit
- Luau code uses StyLua for formatting

## Testing Patterns

Tests use the `Fixture` class from `tests/Fixture.h` with doctest's `TEST_CASE_FIXTURE`:

```cpp
TEST_CASE_FIXTURE(Fixture, "FeatureName")
{
    auto uri = newDocument("test.luau", "local x = 1");
    // Test operations using workspace
}
```

- `newDocument()`: Create and register a test document
- `check()`: Type check source code
- `loadDefinition()`: Load type definition files
- `loadSourcemap()`: Load Rojo sourcemap for Roblox tests
- `sourceWithMarker()`: Parse source with `|` cursor position marker

### Testing with the New Type Solver

When writing tests that require the new Luau type solver (`LuauSolverV2`), use the `ENABLE_NEW_SOLVER()` macro at the start of the test:

```cpp
TEST_CASE_FIXTURE(Fixture, "feature_requiring_new_solver")
{
    ENABLE_NEW_SOLVER();

    auto uri = newDocument("test.luau", "local x = 1");
    // Test code...
}
```

**Important:** Do not use `ScopedFastFlag{FFlag::LuauSolverV2, true}` directly. The Frontend caches the solver mode at construction time, so the `ENABLE_NEW_SOLVER()` macro is required to properly update both the FFlag and the Frontend's cached solver mode.

## Key CMake Targets

- `Luau.LanguageServer`: Static library containing LSP implementation
- `Luau.LanguageServer.CLI`: Executable (`luau-lsp`)
- `Luau.LanguageServer.Test`: Test executable

## CMake Options

- `LUAU_ENABLE_TIME_TRACE`: Enable Luau time tracing
- `LSP_BUILD_ASAN`: Build with AddressSanitizer
- `LSP_STATIC_CRT`: Link with static CRT on Windows
- `LSP_BUILD_WITH_SENTRY`: Enable crash reporting (Windows/macOS only)
- `LSP_WERROR`: Treat warnings as errors (default: ON)
