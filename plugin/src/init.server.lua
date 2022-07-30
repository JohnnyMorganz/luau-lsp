assert(plugin, "This code must run inside of a plugin")

local toolbar = plugin:CreateToolbar("Luau") :: PluginToolbar
local button =
	toolbar:CreateButton("Connect to Language Server", "Connect to Server", "Connect to Server") :: PluginToolbarButton

local connected = false
local connections = {}

local function encodeInstance(instance: Instance)
	local encoded = {}
	encoded.Name = instance.Name
	encoded.ClassName = instance.ClassName
	encoded.Children = {}

	for _, child in pairs(instance:GetChildren()) do
		encoded[child.Name] = encodeInstance(child)
	end
end

local function cleanup()
	for _, connection in pairs(connections) do
		connection:Disconnect()
	end
	connected = false
end

local function watchChanges()
	if connected then
		return
	end
	cleanup()

	connected = true
end

local function handleClick()
	if connected then
		cleanup()
	else
		watchChanges()
	end
end

button.Click:Connect(handleClick)
