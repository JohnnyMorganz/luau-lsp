--!strict
local HttpService = game:GetService("HttpService")
assert(plugin, "This code must run inside of a plugin")

type Settings = {
	port: number,
	startAutomatically: boolean,
	silent: boolean,
	
	include: {Instance}
}

local DefaultPort = 3667
local DefaultIncludeList = {
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
}

if game:GetService("RunService"):IsRunning() then
	return
end

local toolbar = plugin:CreateToolbar("Luau")

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

local SettingsModule = AnalyticsService:FindFirstChild("LuauLSP_Settings") :: ModuleScript

if not SettingsModule then
	SettingsModule = (script :: any).DefaultSettings:Clone()
	assert(SettingsModule, "Luau Typechecking")
	SettingsModule.Name = "LuauLSP_Settings"
	SettingsModule.Parent = AnalyticsService
end

local DefaultSettings = {
	port = DefaultPort,
	startAutomatically = true,
	silent = true,
	
	include = DefaultIncludeList
} :: Settings

local Settings = DefaultSettings :: Settings

assert(SettingsModule, "failed to create settings module")

local function LoadSettings()
	local result, parseError: any = loadstring(SettingsModule.Source)
	if result == nil then
		warn("[Luau Language Server] Could not load settings: " .. parseError)
		Settings = nil
		return
	end
	assert(result, "")
	
	local _, err = pcall(function()
		Settings = result()
	end)
	
	if err then
		warn(err)
	end
	if typeof(Settings) ~= "table" then
		Settings = DefaultSettings
		warn("[Luau Language Server] Could not load settings: invalid settings")
	elseif typeof(Settings.port) ~= "number" then
		Settings.port = DefaultPort
		warn(`[Luau Language Server] Could not load settings: port is set to {DefaultPort}`)
	elseif typeof(Settings.startAutomatically) ~= "boolean" then
		Settings.startAutomatically = false
		warn("[Luau Language Server] Could not load settings: startAutomatically is set to false (automatic)")
	elseif typeof(Settings.silent) ~= "boolean" then
		Settings.silent = true
		warn("[Luau Language Server] Could not load settings: set silent value to false (automatic)")
	elseif typeof(Settings.include) ~= "table" then
		Settings.include = DefaultIncludeList
		warn("[Luau Language Server] Could not load settings: set include list value to default list (automatic)")
	end
end

LoadSettings()

--#endregion

--#region Connect

local ConnectAction = plugin:CreatePluginAction(
	"Luau Language Server Connect",
	"Connect",
	"Connects to Luau Language Server",
	"rbxassetid://11115506617",
	true
)

local connected = Instance.new("BoolValue")
local connections = {}

type EncodedInstance = {
	Name: string,
	ClassName: string,
	Children: { EncodedInstance },
}

local wasConnected

SettingsButton.Click:Connect(function()
	plugin:OpenScript(SettingsModule)
	wasConnected = connected.Value
end)

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
	for _, connection in connections do
		if typeof(connection) == "thread" then
			task.cancel(connection)
		elseif typeof(connection) == "RBXScriptConnection" then
			if not connection.Connected then continue end
			connection:Disconnect() -- Disconnect the connection
		end
	end
	
	connected.Value = false
end

local function sendFullDMInfo()
	local tree = encodeInstance(game, filterServices)

	local success, result = pcall(HttpService.RequestAsync, HttpService, {
		Method = "POST" :: "POST",
		Url = string.format("http://localhost:%s/full", Settings.port),
		Headers = {
			["Content-Type"] = "application/json",
		},
		Body = HttpService:JSONEncode({
			tree = tree,
		}),
	})

	if not success then
		warn(`[Luau Language Server] Connecting to server failed: {result}`)
		cleanup()
	elseif not result.Success then
		warn(`[Luau Language Server] Sending full DM info failed: {result.StatusCode}: {result.Body}`)
		cleanup()
	else
		connected.Value = true
	end
end

local function watchChanges(isSilent)
	local sendTask: thread?

	if connected.Value or Settings == nil then
		if not isSilent then
			warn("[Luau Language Server] Connecting to server failed: invalid settings")
		end
		return
	end

	cleanup()

	local function deferSend()
		if sendTask then
			task.cancel(sendTask)
		end

		sendTask = task.delay(0.5, function()
			sendFullDMInfo()
			sendTask = nil
		end)
	end

	-- TODO: we should only send delta info if possible
	local function bindConnections(instance: Instance): (RBXScriptConnection, RBXScriptConnection)
		local nameConnection = instance:GetPropertyChangedSignal("Name"):Connect(function()
			if not Settings.silent then
				print("[Luau Language Server] Instance updated, check vs code")
			end
			
			deferSend()
		end)

		local ancestryConnection: RBXScriptConnection; ancestryConnection = instance.AncestryChanged:Connect(function(_, parent)
			if parent then
				deferSend()
				return
			end

			if not Settings.silent then
				print("[Luau Language Server] Instance removed, no changes will apply on it")
			end
		
			nameConnection:Disconnect()
			ancestryConnection:Disconnect()
			
			deferSend()
		end) :: RBXScriptConnection
		
		return nameConnection, ancestryConnection
	end
	
	local function descendantChanged(instance: Instance)
		for _, service in Settings.include do
			if instance:IsDescendantOf(service) then -- Parent == nil means it got removed
				local nameConnection = instance:GetPropertyChangedSignal("Name"):Connect(function()
					if not Settings.silent then
						print("[Luau Language Server] Updated instance")
					end
					
					deferSend()
				end)
				
				local nameConnection, ancestryConnection = bindConnections(instance)
				
				table.insert(connections, nameConnection)
				table.insert(connections, ancestryConnection)
				
				deferSend()
				return
			end
		end
	end
	
	local instanceConnections = {}

	local function updateChanges()
		for _, service in Settings.include do
			for _, instance: Instance in service:GetDescendants() do
				local nameConnection, ancestryConnection = bindConnections(instance)

				table.insert(connections, nameConnection)
				table.insert(connections, ancestryConnection)
			end
		end
	end

	table.insert(connections, game.DescendantAdded:Connect(descendantChanged))
	table.insert(connections, task.spawn(updateChanges))

	sendFullDMInfo()
end

function connectServer()
	if connected.Value then
		if not Settings.silent then
			print("[Luau Language Server] Disconnecting from DataModel changes")
		end

		cleanup()
	else
		if not Settings.silent then
			print("[Luau Language Server] Connecting to server")
		end

		watchChanges(Settings.silent)
	end
end

ConnectButton.Click:Connect(connectServer)
ConnectAction.Triggered:Connect(connectServer)

connected.Changed:Connect(function()
	ConnectButton.Icon = if connected.Value then "rbxassetid://11116536087" else "rbxassetid://11115506617"
	
	if connected.Value then
		if not Settings.silent then
			print(`[Luau Language Server] Connected to the server, listening on port {Settings.port}`)
		end
	end
end)

if Settings and Settings.startAutomatically then
	connectServer()
end

SettingsModule.Changed:Connect(function()
	if connected.Value then
		cleanup()
	end
	
	LoadSettings()
	
	if wasConnected then
		connectServer(true)
	end
end)

--#endregion
