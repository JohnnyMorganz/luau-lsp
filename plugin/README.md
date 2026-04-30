# Luau Language Server Studio Plugin

A Roblox Studio plugin that provides real-time DataModel information to the [Luau Language Server](https://github.com/JohnnyMorganz/luau-lsp), enabling accurate autocompletion and type analysis for instances in external editors.

## Overview

This plugin acts as a bridge between Roblox Studio and an external Luau Language Server. It tracks changes to instances in your game's DataModel and sends serialized tree updates to the language server via HTTP, allowing external editors like VS Code to provide intelligent autocompletion for Roblox-specific types.

## Installation

1. Install the plugin in Roblox Studio [(Get it from the Creator Store)](https://www.roblox.com/library/10913122509/Luau-Language-Server-Companion)
2. Ensure the Luau Language Server extension is installed in your editor (e.g., VS Code)
3. Enable the plugin feature in your editor settings:
   - VS Code: Set `luau-lsp.plugin.enabled` to `true`

## Usage

### Connecting to the Language Server

1. Click the **Connect** button in the Plugin ribbon to toggle the connection
2. Alternatively, use the keyboard shortcut via the plugin action
3. The toolbar icon will show a green indicator when connected

### Configuring Settings

Click the **Settings** button to open the configuration module (`LuauLSP_Settings` in TestService). Available options:

| Setting              | Type         | Default              | Description                                                             |
| -------------------- | ------------ | -------------------- | ----------------------------------------------------------------------- |
| `host`               | `string`     | `"http://localhost"` | Language server hostname                                                |
| `port`               | `number`     | `3667`               | Language server port (configured via `luau-lsp.plugin.port` in VS Code) |
| `startAutomatically` | `boolean`    | `false`              | Auto-connect when Studio starts                                         |
| `include`            | `{Instance}` | Core services        | Instances to track for changes                                          |
| `logLevel`           | `string`     | `"INFO"`             | Logging verbosity (`DEBUG`, `INFO`, `WARN`, `ERROR`)                    |

### Default Tracked Services

By default, the plugin tracks these services:

- Workspace
- Players
- Lighting
- ReplicatedFirst
- ReplicatedStorage
- ServerScriptService
- ServerStorage
- StarterGui
- StarterPack
- StarterPlayer
- SoundService
- Chat
- LocalizationService
- TestService

## Architecture

```
src/
├── init.server.luau      # Main entry point and UI setup
├── LSPManager.luau       # Connection lifecycle and server communication
├── InstanceTracker.luau  # DataModel change detection and encoding
├── ServerEndpoints.luau  # HTTP request handling
├── types.luau            # Type definitions
├── Assets.luau           # Toolbar icon asset IDs
├── Settings/
│   ├── init.luau         # Settings management with live reload
│   └── DefaultSettings.luau  # Default configuration values
└── Utils/
    ├── Debounce.luau     # Rate-limiting utility
    ├── Log.luau          # Leveled logging system
    └── Signal.luau       # Event/callback management
```
