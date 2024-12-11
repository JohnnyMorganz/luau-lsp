local HttpService = game:GetService("HttpService")
local RunService = game:GetService("RunService")
local ScriptEditorService = game:GetService("ScriptEditorService")
local TestService = game:GetService("TestService")

local IsRunning = RunService:IsRunning()
if IsRunning then
	return
end
assert(plugin, "This code must run inside of a plugin")

local SETTINGS_MODULE_NAME = "LuauLanguageServer_Settings"

--// List of classes who's data will not be updated, due to lag more often then not.
local CLASSES_EXCEPTION_LIST = {
	"Camera", --(CFrame)
	"Player", --(CloudEditCameraCoordinateFrame)
}

local DEFAULT_SOURCE = [[
return {
    --// Should be unique to Rojo port (different).
	Port = 3667,

    --// Decides whether or not the companion plugin automatically starts listening on studio launch.
	StartAutomatically = false,

    --// Setting to true will enable verbose error messages and warns. Currently does nothing.
    DebugMode = false,

    --// List of instances who's descendants will be encoded and sent to VS Code.
	Include = {
		game:GetService("Workspace"),
		game:GetService("Players"),
		game:GetService("Lighting"),
		game:GetService("ReplicatedFirst"),
		game:GetService("ReplicatedStorage"),
		game:GetService("ServerScriptService"),
		game:GetService("ServerStorage"),
		game:GetService("StarterGui"),
		game:GetService("StarterPack"),
		game:GetService("StarterPlayer"),
		game:GetService("SoundService"),
		game:GetService("Chat"),
		game:GetService("LocalizationService"),
		game:GetService("TestService"),
	},
}]]

local Connections: { RBXScriptConnection } = {}
local Connected = false

type EncodedInstance = {
	Name: string,
	ClassName: string,
	Children: { EncodedInstance? },
}

local Toolbar = plugin:CreateToolbar("Luau Language Server Test")
local ConnectAction = plugin:CreatePluginAction(
	"Luau Language Server Connect",
	"Connect",
	"Connects to Luau Language Server",
	"rbxassetid://11115506617",
	true
)
local ConnectButton = Toolbar:CreateButton(
	"Luau Language Server Setup",
	"Toggle Menu",
	"rbxassetid://11115506617",
	"Luau Language Server"
)
local SettingsButton = Toolbar:CreateButton("Settings", "Open Settings", "rbxassetid://13997395868")

local DefaultSettingsModule = Instance.new("ModuleScript")
DefaultSettingsModule.Name = SETTINGS_MODULE_NAME
ScriptEditorService:UpdateSourceAsync(DefaultSettingsModule, function()
	return DEFAULT_SOURCE
end)
local DefaultSettings: {
	Port: number,
	StartAutomatically: boolean,
	DebugMode: boolean,
	Include: boolean,
} =
	require(DefaultSettingsModule)
--// Was just much easier to create a permanent reference for the default settings config

local function GetAndValidateSettingsModule()
	local SettingsModule = (
		TestService:FindFirstChild(SETTINGS_MODULE_NAME) or Instance.new("ModuleScript")
	) :: ModuleScript
	local SettingsModuleSource = ScriptEditorService:GetEditorSource(SettingsModule)
	if SettingsModuleSource == "" or SettingsModuleSource == nil then
		SettingsModule = DefaultSettingsModule:Clone()
		warn(
			"[Luau Language Server] Could not load settings: Unable to find saved settings, reverting to default settings."
		)
	end

	--// No loss in not running a check here since Luau runs an internal validation for set
	SettingsModule.Name = SETTINGS_MODULE_NAME
	SettingsModule.Parent = TestService

	local Settings = require(SettingsModule)
	if typeof(Settings) ~= "table" then
		warn(
			"[Luau Language Server] Could not load settings: Settings module does not return a table. Defaulting to default settings."
		)
		SettingsModule = DefaultSettingsModule:Clone()
	elseif typeof(Settings.Port) ~= "number" then
		warn(
			`[Luau Language Server] Could not load settings: Port is not a number. Defaulting to {DefaultSettings.Port}.`
		)
		Settings.Port = DefaultSettings.Port
	elseif type(Settings.StartAutomatically) ~= "boolean" then
		warn(
			`[Luau Language Server] Could not load settings: StartAutomatically is not a boolean. Defaulting to {DefaultSettings.Port}.`
		)
		Settings.StartAutomatically = DefaultSettings.StartAutomatically
	elseif type(Settings.Include) ~= "table" then
		warn(
			`[Luau Language Server] Could not load settings: Include list is not a table. Defaulting to {DefaultSettings.Include}}.`
		)
		Settings.Include = DefaultSettings.Include
	end

	return SettingsModule
end

local SettingsModule = GetAndValidateSettingsModule()
local Settings = require(SettingsModule)

local function FilterServices(Child: Instance): boolean
	return not not table.find(Settings.Include, Child)
end

local function EncodeInstance(Instance: Instance, ChildFilter: ((Instance) -> boolean)?): EncodedInstance
	local Encoded = {}
	Encoded.Name = Instance.Name or "EmptyName"
	Encoded.ClassName = Instance.ClassName or "EmptyClassName"
	Encoded.Children = {}

	for _, Child in Instance:GetChildren() do
		if ChildFilter and not ChildFilter(Child) then
			continue
		end

		table.insert(Encoded.Children, EncodeInstance(Child))
	end

	return Encoded
end

local function CleanUpConnections()
	local WasConnected = Connected
	for _, Connection in Connections do
		Connection:Disconnect()
	end
	Connected = false
	ConnectButton.Icon = "rbxassetid://11115506617"

	if WasConnected then
		warn("[Luau Language Server] No longer sending DataModel information.")
	end
end

local function SendDataModelInfo()
	local Tree = EncodeInstance(game, FilterServices)

	local Success, Result = pcall(HttpService.RequestAsync, HttpService, {
		Method = "POST" :: "POST",
		Url = string.format("http://localhost:%s/full", tostring(Settings.Port)),
		Headers = {
			["Content-Type"] = "application/json",
		},
		Body = HttpService:JSONEncode({
			tree = Tree,
		}),

		Compress = Enum.HttpCompression.Gzip,
	})

	if not Success then
		warn(`[Luau Language Server] Connecting to server failed: {Result}`)
		CleanUpConnections()
	elseif not Result.Success then
		warn(`[Luau Language Server] Sending DataModel info failed: {Result.StatusCode}: {Result.Body}`)
		CleanUpConnections()
	else
		Connected = true
		ConnectButton.Icon = "rbxassetid://11116536087"
		warn("[Luau Language Server] Now sending DataModel information.")
	end
end

local function WatchChanges()
	local SendTask: thread?
	if Connected or Settings == nil then
		warn("[Luau Language Server] Connecting to server failed: Already connected, or settings non-existent.")
		return
	end
	CleanUpConnections()

	local function DeferSendDataModelInfo()
		if SendTask then
			task.cancel(SendTask)
		end
		SendTask = task.delay(0.5, function()
			SendDataModelInfo()
			SendTask = nil
		end)
	end

	for _, Service: Instance in Settings.Include do
		table.insert(
			Connections,
			Service.DescendantAdded:Connect(function(Descendant: Instance)
				if table.find(CLASSES_EXCEPTION_LIST, Descendant.ClassName) ~= nil then
					return
				end
				--print(`[Luau Language Server] {Descendant.Name} added to {Service.ClassName}`)
				table.insert(
					Connections,
					Descendant.Changed:Connect(function(Property: string)
						--print(`[Luau Language Server] {Property} changed on {Descendant.Name}`) Re enable for Verbose of some sort
						DeferSendDataModelInfo()
					end)
				)
			end)
		)
		for _, Descendant in Service:GetDescendants() do
			if table.find(CLASSES_EXCEPTION_LIST, Descendant.ClassName) ~= nil then
				continue
			end
			table.insert(
				Connections,
				Descendant.Changed:Connect(function(Property: string)
					print(`{Property} changed on {Descendant.Name}`)
					DeferSendDataModelInfo()
				end)
			)
		end
	end
	SendDataModelInfo()
end

function ToggleServerConnection()
	if Connected then
		CleanUpConnections()
	else
		WatchChanges()
	end
end

ConnectButton.Click:Connect(ToggleServerConnection)
ConnectAction.Triggered:Connect(ToggleServerConnection)

SettingsButton.Click:Connect(function()
	plugin:OpenScript(SettingsModule)
end)

SettingsModule:GetPropertyChangedSignal("Source"):Connect(function()
	local WasConnected = Connected
	if Connected then
		CleanUpConnections()
	end
	SettingsModule = GetAndValidateSettingsModule()
	Settings = require(SettingsModule)

	if WasConnected then
		ToggleServerConnection()
	end
end)

if Settings and Settings.StartAutomatically then
	ToggleServerConnection()
end
