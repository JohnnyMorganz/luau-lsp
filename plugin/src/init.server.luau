--!strict

--- Luau Language Server Plugin
--- Main entry point for the Roblox Studio plugin.
---
--- This plugin connects to an external Luau Language Server to provide
--- enhanced IDE features. It tracks changes to the DataModel and sends
--- updates to the language server for accurate autocompletion and analysis.
---
--- For setup instructions, see:
--- https://github.com/JohnnyMorganz/luau-lsp

assert(plugin, "This code must run inside of a plugin")
if game:GetService("RunService"):IsRunning() then
	return
end

local Assets = require(script.Assets)
local InstanceTracker = require(script.InstanceTracker)
local LSPManager = require(script.LSPManager)
local Log = require(script.Utils.Log)
local Settings = require(script.Settings)

local toolbar = plugin:CreateToolbar("Luau")

--#region Setup

--- Create the settings manager
local settings = Settings.new(script.Settings.DefaultSettings)

--- Create the instance tracker with an include checker that uses current settings
local instanceTracker = InstanceTracker.new(function(): { Instance }
	return settings:get("include")
end)

--- Create the LSP manager with injected dependencies
local lspManager = LSPManager.new(settings, instanceTracker)

--- Handle settings changes by updating the LSP connection.
--- If already connected, silently reconnect with new settings.
settings.onChanged:connect(function()
	Log.debug("Settings changed")

	lspManager:setAddress(settings:get("host"), settings:get("port"))

	if lspManager:isConnected() then
		-- Silently reload with the new settings
		Log.setLevel(Log.Level.WARN)

		lspManager:disconnect()
		lspManager:connect()
	end

	local logLevel = settings:get("logLevel")
	if not Log.Level[logLevel] then
		Log.warn(`Invalid log level: {logLevel}. Defaulting to INFO`)
		logLevel = "INFO"
	end

	Log.setLevel(Log.Level[logLevel])
end)

--- Initialize all systems
settings:init()
lspManager:init()

-- Auto-connect if set.
if settings:get("startAutomatically") then
	Log.debug("Starting automatically")
	lspManager:connect()
end

--- Clean up connections when the plugin is unloaded.
plugin.Unloading:Connect(function()
	lspManager:unload()
	settings:unload()
end)

--#endregion

--#region User Interface

--- Toggles the connection to the language server.
--- Guards against toggling when settings aren't loaded.
local function toggleConnection()
	if not settings:isLoaded() then
		Log.warn("Settings are not loaded, cannot toggle connection")
		return
	end

	if lspManager:isConnected() then
		lspManager:disconnect()
	else
		lspManager:connect()
	end
end

--- Main toolbar button for toggling the language server connection.
local ConnectButton = toolbar:CreateButton(
	"Luau Language Server Setup",
	"Toggle Menu",
	if lspManager:isConnected() then Assets.ConnectedIcon else Assets.Icon,
	"Luau Language Server"
) :: PluginToolbarButton
ConnectButton.ClickableWhenViewportHidden = true
ConnectButton.Click:Connect(toggleConnection)

--- Update the toolbar icon based on connection state.
lspManager.onConnectionChanged:connect(function(isConnected: boolean)
	ConnectButton.Icon = if isConnected then Assets.ConnectedIcon else Assets.Icon
end)

--- Plugin action for keyboard shortcut support.
local ConnectAction = plugin:CreatePluginAction(
	"Luau Language Server Connect",
	"Connect",
	"Connects to Luau Language Server",
	Assets.Icon,
	true
)
ConnectAction.Triggered:Connect(toggleConnection)

--- Settings button for opening the configuration module.
local SettingsButton = toolbar:CreateButton("Settings", "Open Settings", Assets.SettingsIcon) :: PluginToolbarButton
SettingsButton.ClickableWhenViewportHidden = true
SettingsButton.Click:Connect(function()
	settings:edit()
end)

--#endregion
