--!strict
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

local Toolbar = plugin:CreateToolbar("Luau Language Server")
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
	require(DefaultSettingsModule) :: any
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

	local Settings = require(SettingsModule) :: any
	print(Settings)
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
local Settings = require(SettingsModule) :: any

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

local function CleanUp()
	for _, Connection in Connections do
		Connection:Disconnect()
	end
	Connected = false
	ConnectButton.Icon = "rbxassetid://11115506617"
end

local function SendDataModelInfo(InternalUpdate: boolean?)
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
		CleanUp()
	elseif not Result.Success then
		warn(`[Luau Language Server] Sending DataModel info failed: {Result.StatusCode}: {Result.Body}`)
		CleanUp()
	else
		if not InternalUpdate then
			warn("[Luau Language Server] Connected to server.")
		end
		Connected = true
		ConnectButton.Icon = "rbxassetid://11116536087"
	end
end

local function WatchChanges(InternalUpdate: boolean?)
	local SendTask: thread?
	if Connected or Settings == nil then
		warn("[Luau Language Server] Connecting to server failed: Already connected, or settings are blank.")
		return
	end
	CleanUp()

	local function DeferSendDataModelInfo()
		if SendTask then
			task.cancel(SendTask)
		end
		SendTask = task.delay(0.5, function()
			SendDataModelInfo(true)
			SendTask = nil
		end)
	end

	local function SetupListeners(Instance: Instance)
		local NameChangedConnection: RBXScriptConnection = nil
		NameChangedConnection = Instance:GetPropertyChangedSignal("Name"):Connect(function()
			print(Settings.DebugMode)
			if Settings.DebugMode then
				print(`[Luau Language Server] Name changed on {Instance.Name}`)
			end
			DeferSendDataModelInfo()
		end)
		table.insert(Connections, NameChangedConnection)

		local ParentChangedConnection: RBXScriptConnection = nil
		ParentChangedConnection = Instance:GetPropertyChangedSignal("Parent"):Connect(function()
			if Instance.Parent == nil and Settings.DebugMode then
				print(`[Luau Language Server] {Instance.Name} deleted from DataModel.`)
			end

			ParentChangedConnection:Disconnect()
			NameChangedConnection:Disconnect()
			DeferSendDataModelInfo()
		end)
	end

	for _, Service: Instance in Settings.Include do
		Service.DescendantAdded:Connect(function(Descendant: Instance)
			if Settings.DebugMode then
				print(`[Luau Language Server] {Descendant.Name} parented to {Descendant.Parent}`)
			end
			SetupListeners(Descendant)
		end)

		for _, Descendant in Service:GetDescendants() do
			SetupListeners(Descendant)
		end
	end
	SendDataModelInfo(InternalUpdate)
end

function ToggleServerConnection(InternalUpdate: boolean?)
	SettingsModule = GetAndValidateSettingsModule()
	Settings = require(SettingsModule) :: any

	if Connected then
		if not InternalUpdate then
			warn("[Luau Language Server] Disconnected from server.")
		elseif InternalUpdate and Settings.DebugMode then
			warn("[Luau Language Server] Disconnected from server (internally).")
		end
		CleanUp()
	else
		WatchChanges(InternalUpdate)
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
		CleanUp()
	end
	SettingsModule = GetAndValidateSettingsModule()
	Settings = require(SettingsModule) :: any
	print(Settings)

	if WasConnected then
		if Settings.DebugMode then
			warn("[Luau Language Server] No longer sending DataModel information.")
		end
		ToggleServerConnection(true)
	end
end)

if Settings and Settings.StartAutomatically then
	warn("[Luau Language Server] Companion plugin automatically connecting.")
	ToggleServerConnection()
end
