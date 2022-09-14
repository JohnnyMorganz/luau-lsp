--!strict
local HttpService = game:GetService("HttpService")
assert(plugin, "This code must run inside of a plugin")

local toolbar = plugin:CreateToolbar("Luau") :: PluginToolbar
local button =
	toolbar:CreateButton("Connect to Language Server", "Connect to Server", "Connect to Server") :: PluginToolbarButton

local connected = false
local connections = {}

local INCLUDED_SERVICES = {
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

type EncodedInstance = {
	Name: string,
	ClassName: string,
	Children: { EncodedInstance },
}

local function filterServices(child: Instance): boolean
	return not not table.find(INCLUDED_SERVICES, child)
end

local function encodeInstance(instance: Instance, childFilter: ((Instance) -> boolean)?): EncodedInstance
	local encoded = {}
	encoded.Name = instance.Name
	encoded.ClassName = instance.ClassName
	encoded.Children = {}

	for _, child in pairs(instance:GetChildren()) do
		if childFilter and not childFilter(child) then
			continue
		end

		local success, data = pcall(encodeInstance, child)
		if success then
			table.insert(encoded.Children, data)
		end
	end

	return encoded
end

local function cleanup()
	for _, connection in pairs(connections) do
		connection:Disconnect()
	end
	connected = false
end

local function sendFullDMInfo()
	local tree = encodeInstance(game, filterServices)

	local result = HttpService:RequestAsync({
		Method = "POST",
		Url = "http://localhost:3667/full",
		Headers = {
			["Content-Type"] = "application/json",
		},
		Body = HttpService:JSONEncode({
			tree = tree,
		}),
	})

	if not result.Success then
		warn("[Luau Language Server] Sending full DM info failed: " .. result.StatusCode .. ": " .. result.Body)
	end
end

local function watchChanges()
	if connected then
		return
	end
	cleanup()

	connected = true

	-- TODO: we should only send delta info if possible
	local function descendantChanged(instance: Instance)
		for _, service in INCLUDED_SERVICES do
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

local function handleClick()
	if connected then
		print("[Luau Language Server] Disconnecting from DataModel changes")
		cleanup()
	else
		print("[Luau Language Server] Listening for DataModel changes")
		watchChanges()
	end
end

button.Click:Connect(handleClick)
