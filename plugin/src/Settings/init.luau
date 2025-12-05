--!strict

--- Manages plugin settings stored in a ModuleScript in TestService.
--- Settings are loaded from source code and validated against defaults.
---
--- The settings module is automatically created if it doesn't exist,
--- using DefaultSettings as a template. Changes to the settings module
--- are detected and reloaded automatically with debouncing.
---
--- Usage:
--- ```lua
--- local settings = Settings.new(script.DefaultSettings)
--- settings:init()
--- local host = settings:get("host")
--- ```

local TestService = game:GetService("TestService")
local ScriptEditorService = game:GetService("ScriptEditorService")

local Log = require(script.Parent.Utils.Log)
local Debounce = require(script.Parent.Utils.Debounce)
local Signal = require(script.Parent.Utils.Signal)
local types = require(script.Parent.types)

local Settings = {}
Settings.__index = Settings

--- Internal data structure for a Settings instance.
type SettingsData = {
	--- The currently loaded settings values, or nil if not loaded.
	_values: types.Settings?,
	--- Reference to the settings ModuleScript in TestService.
	_module: ModuleScript?,
	--- Reference to the default settings module for cloning.
	_defaultSettingsModule: ModuleScript,
	--- The default settings values loaded from the module.
	_defaultSettings: types.Settings,
	--- Connection for detecting changes to the settings module source.
	_moduleChangedConnection: RBXScriptConnection?,
	--- Debounce controller for rate-limiting settings reloads.
	_loadDebounce: Debounce.Debounce?,
	--- Signal fired when settings change.
	onChanged: Signal.Signal<types.Settings?>,
}

--- A settings manager that loads and watches configuration.
export type Settings = typeof(setmetatable({} :: SettingsData, Settings))

--- Creates a new Settings instance.
--- @param defaultSettingsModule -- The ModuleScript containing default settings.
--- @return Settings -- A new Settings instance ready for initialization.
function Settings.new(defaultSettingsModule: ModuleScript): Settings
	local self = {
		_values = nil,
		_module = nil,
		_defaultSettingsModule = defaultSettingsModule,
		_defaultSettings = require(defaultSettingsModule) :: types.Settings,
		_moduleChangedConnection = nil,
		_loadDebounce = nil,
		onChanged = Signal.new() :: Signal.Signal<types.Settings?>,
	}
	return setmetatable(self, Settings)
end

--- Initializes the settings system.
--- Creates the settings module if it doesn't exist and starts
--- watching for changes. Must be called before using other Settings functions.
--- @param self -- The Settings instance.
function Settings.init(self: Settings): ()
	self._module = TestService:FindFirstChild("LuauLSP_Settings") :: ModuleScript?

	if not self._module then
		local newModule = self._defaultSettingsModule:Clone() :: ModuleScript
		newModule.Name = "LuauLSP_Settings"
		newModule.Parent = TestService
		self._module = newModule
	end
	assert(self._module, "Failed to get settings module")

	self._loadDebounce = Debounce.new(0.75, function()
		self:load()
	end)

	self._moduleChangedConnection = self._module:GetPropertyChangedSignal("Source"):Connect(function()
		-- Defer to avoid reloading on every keystroke
		self:deferredLoad()
	end)

	self:load()
end

--- Returns whether settings have been successfully loaded.
--- @param self -- The Settings instance.
--- @return boolean -- True if settings are available.
function Settings.isLoaded(self: Settings): boolean
	return self._values ~= nil
end

--- Opens the settings module in the Script Editor for editing.
--- @param self -- The Settings instance.
function Settings.edit(self: Settings): ()
	assert(self._module, "Settings must be initialized before editing")
	ScriptEditorService:OpenScriptDocumentAsync(self._module)
end

--- Cleans up the settings system.
--- Disconnects change listeners and clears loaded values.
--- Call this when the plugin is unloading.
--- @param self -- The Settings instance.
function Settings.unload(self: Settings): ()
	if self._moduleChangedConnection then
		self._moduleChangedConnection:Disconnect()
		self._moduleChangedConnection = nil
	end
	if self._loadDebounce then
		self._loadDebounce:cancel()
	end
	self.onChanged:destroy()
	self._values = nil
end

--- Loads and validates settings from the settings module.
--- Invalid settings are rejected with a warning and the previous
--- values are retained. Valid settings are merged with defaults.
--- @param self -- The Settings instance.
function Settings.load(self: Settings): ()
	assert(self._module, "Settings must be initialized before loading")

	local settingsMethod, parseError = loadstring(self._module.Source, self._module.Name)
	if settingsMethod == nil then
		Log.warn("Could not load settings:", parseError)
		return
	end

	local success, result = pcall(settingsMethod)
	if not success then
		Log.warn("Could not load settings:", result)
		return
	end

	if type(result) ~= "table" then
		Log.warn("Could not load settings: invalid settings")
		return
	end

	-- Merge user settings with defaults to ensure all keys exist
	local mergedSettings = table.clone(self._defaultSettings)
	for key, value in result do
		local expectedType = typeof(self._defaultSettings[key])
		if expectedType ~= "nil" and expectedType ~= typeof(value) then
			Log.warn(`Could not load settings: invalid type for {key}. Expected {expectedType}, got {typeof(value)}`)
			return
		end

		mergedSettings[key] = value
	end

	self._values = mergedSettings
	self.onChanged:fire(self._values)
end

--- Schedules a deferred settings reload using debouncing.
--- Multiple rapid changes will be batched into a single reload.
--- @param self -- The Settings instance.
--- @return thread -- The scheduled task thread.
function Settings.deferredLoad(self: Settings): thread
	assert(self._loadDebounce, "Settings must be initialized before use")
	return self._loadDebounce:call()
end

--- Retrieves a setting value by key.
--- Returns the loaded value if available, otherwise the default.
--- @param self -- The Settings instance.
--- @param key -- The setting key to retrieve.
--- @return any -- The setting value.
function Settings.get(self: Settings, key: string): any
	if self._values and self._values[key] ~= nil then
		return self._values[key]
	end

	local defaultValue = self._defaultSettings[key]
	if defaultValue == nil then
		Log.warn(`Could not get setting: unknown key '{key}'`)
	end

	return defaultValue
end

return Settings
