--!nocheck
local HttpService = game:GetService("HttpService")
local RunService = game:GetService("RunService")
local ServerStorage = game:GetService("ServerStorage")

local IsRunning = RunService:IsRunning()
if IsRunning then return end
assert(plugin, "This code must run inside of a plugin")

local SETTINGS_MODULE_NAME = "LuauLSP_Settings"
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
}
]]

local Connections: {RBXScriptConnection} = {}
local Connected = false

type EncodedInstance = {
	Name: string,
	ClassName: string,
	Children: { EncodedInstance },
}

local Toolbar = plugin:CreateToolbar("Luau LSP Test")
local ConnectAction = plugin:CreatePluginAction("Luau LSP Connect", "Connect", "Connects to Luau LSP", "rbxassetid://11115506617", true)
local ConnectButton = Toolbar:CreateButton("Luau LSP Setup", "Toggle Menu", "rbxassetid://11115506617", "Luau LSP")
local SettingsButton = Toolbar:CreateButton("Settings", "Open Settings", "rbxassetid://13997395868")

local function GetAndValidateSettingsModule()
    local SettingsModule = (game:FindFirstChild(SETTINGS_MODULE_NAME, true) or Instance.new("ModuleScript")) :: ModuleScript
    if SettingsModule.Source == "" or SettingsModule.Source == nil then
        SettingsModule.Source = DEFAULT_SOURCE
        warn("[Luau LSP] Could not load settings: Unable to find saved settings, reverting to default settings.")
    end

    --// No loss in not running a check here since Luau runs an internal validation for set
    SettingsModule.Name = SETTINGS_MODULE_NAME
    SettingsModule.Parent = ServerStorage

    local Settings = require(SettingsModule)
    if typeof(Settings) ~= "table" then
        error("[Luau LSP] Could not load settings: Settings module does not return a table.")
    elseif typeof(Settings.Port) ~= "number" then
        error("[Luau LSP] Could not load settings: Port is not a number.")
    elseif type(Settings.StartAutomatically) ~= "boolean" then
        error("[Luau LSP] Could not load settings: StartAutomatically is not a boolean.")
    elseif type(Settings.Include) ~= "table" then
        error("[Luau LSP] Could not load settings: Include list is not a table.")
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
	Encoded.Name = Instance.Name
	Encoded.ClassName = Instance.ClassName
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
		warn("[Luau LSP] No longer sending DataModel information.")
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
		warn(`[Luau LSP] Connecting to server failed: {Result}`)
		CleanUpConnections()
	elseif not Result.Success then
		warn(`[Luau LSP] Sending DataModel info failed: {Result.StatusCode}: {Result.Body}`)
		CleanUpConnections()
	else
		Connected = true
		ConnectButton.Icon = "rbxassetid://11116536087"
        warn("[Luau LSP] Now sending DataModel information.")
	end
end

local function WatchChanges()
	local SendTask: thread?
	if Connected or Settings == nil then
		warn("[Luau LSP] Connecting to server failed: Already connected, or settings non-existent.")
		return
	end
	CleanUpConnections()

	local function DeferSend()
		if SendTask then
			task.cancel(SendTask)
		end
		SendTask = task.delay(0.5, function()
			SendDataModelInfo()
			SendTask = nil
		end)
	end

	-- TODO: we should only send delta info if possible
	local function DescendantChanged(Instance: Instance)
		for _, Service in Settings.Include do
			if Instance:IsDescendantOf(Service) then
				DeferSend()
				return
			end
		end
	end

	table.insert(Connections, game.DescendantAdded:Connect(DescendantChanged))
	table.insert(Connections, game.DescendantRemoving:Connect(DescendantChanged))
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
