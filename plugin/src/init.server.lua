--!strict
local HttpService = game:GetService("HttpService")
assert(plugin, "This code must run inside of a plugin")

if game:GetService("RunService"):IsRunning() then
	return
end

local toolbar = plugin:CreateToolbar("Luau") :: PluginToolbar

local ConnectButton = toolbar:CreateButton(
	"Luau Language Server Setup",
	"Toggle Menu",
	"rbxassetid://11115506617",
	"Luau Language Server"
) :: PluginToolbarButton

local SettingsButton =
	toolbar:CreateButton("Settings", "Open Settings", "rbxassetid://13997395868") :: PluginToolbarButton

--#region Settings

local AnalyticsService = game:GetService("AnalyticsService")

local Settings = nil :: any

local SettingsModule = AnalyticsService:FindFirstChild("LuauLSP_Settings")
if not SettingsModule then
	SettingsModule = script.DefaultSettings:Clone()
	SettingsModule.Name = "LuauLSP_Settings"
	SettingsModule.Parent = AnalyticsService
end

local function LoadSettings()
	local result, parseError: any = loadstring(SettingsModule.Source)
	if result == nil then
		warn("[Luau Language Server] Could not load settings: " .. parseError)
		Settings = nil
		return
	end
	pcall(function()
		Settings = result()
	end)
	if type(Settings) ~= "table" then
		Settings = nil
		warn("[Luau Language Server] Could not load settings: invalid settings")
	elseif type(Settings.port) ~= "number" then
		Settings = nil
		warn("[Luau Language Server] Could not load settings: invalid port")
	elseif type(Settings.startAutomatically) ~= "boolean" then
		Settings = nil
		warn("[Luau Language Server] Could not load settings: invalid startAutomatically value")
	elseif type(Settings.include) ~= "table" then
		Settings = nil
		warn("[Luau Language Server] Could not load settings: invalid include list")
	end
end

LoadSettings()

SettingsButton.Click:Connect(function()
	plugin:OpenScript(SettingsModule)
end)

--#endregion

--#region Connect

local ConnectAction = plugin:CreatePluginAction(
	"Luau Language Server Connect",
	"Connect",
	"Connects to Luau Language Server",
	"rbxassetid://11115506617",
	true
)

local connected: BoolValue = Instance.new("BoolValue")
local connections = {}

type EncodedInstance = {
	Name: string,
	ClassName: string,
	Children: { EncodedInstance },
}

local function filterServices(child: Instance): boolean
	return not not table.find(Settings.include, child)
end

local function encodeInstance(instance: Instance, childFilter: ((Instance) -> boolean)?): EncodedInstance
	local encoded = {}
	encoded.Name = instance.Name
	encoded.ClassName = instance.ClassName
	encoded.Children = {}

	for _, child in instance:GetChildren() do
		if childFilter and not childFilter(child) then
			continue
		end

		table.insert(encoded.Children, encodeInstance(child))
	end

	return encoded
end

local function cleanup()
	for _, connection in pairs(connections) do
		connection:Disconnect()
	end
	connected.Value = false
end

local function sendFullDMInfo()
	local tree = encodeInstance(game, filterServices)

	local success, result = pcall(HttpService.RequestAsync, HttpService, {
		Method = "POST",
		Url = string.format("http://localhost:%s/full", Settings.port),
		Headers = {
			["Content-Type"] = "application/json",
		},
		Body = HttpService:JSONEncode({
			tree = tree,
		}),
	})

	if not success then
		warn("[Luau Language Server] Connecting to server failed: " .. result)
		connected.Value = false
	elseif not result.Success then
		warn("[Luau Language Server] Sending full DM info failed: " .. result.StatusCode .. ": " .. result.Body)
		connected.Value = false
	else
		print("[Luau Language Server] Listening for DataModel changes")
		connected.Value = true
	end
end

local function watchChanges()
	if connected.Value or Settings == nil then
		warn("[Luau Language Server] Connecting to server failed: invalid settings")
		return
	end
	cleanup()

	-- TODO: we should only send delta info if possible
	local function descendantChanged(instance: Instance)
		for _, service in Settings.include do
			if instance:IsDescendantOf(service) then
				sendFullDMInfo()
				return
			end
		end
	end
	table.insert(connections, game.DescendantAdded:Connect(descendantChanged))
	table.insert(connections, game.DescendantRemoving:Connect(descendantChanged))

	sendFullDMInfo()
end

function connectServer()
	if connected.Value then
		print("[Luau Language Server] Disconnecting from DataModel changes")
		cleanup()
	else
		print("[Luau Language Server] Connecting to server")
		watchChanges()
	end
end

ConnectButton.Click:Connect(connectServer)

ConnectAction.Triggered:Connect(connectServer)

connected.Changed:Connect(function()
	ConnectButton.Icon = if connected.Value then "rbxassetid://11116536087" else "rbxassetid://11115506617"
end)

if Settings and Settings.startAutomatically then
	connectServer()
end

SettingsModule.Changed:Connect(function()
	if connected.Value then
		print("[Luau Language Server] Disconnecting from DataModel changes")
		cleanup()
	end
	LoadSettings()
end)

--#endregion
