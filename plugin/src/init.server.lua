--!strict
local HttpService = game:GetService("HttpService")
assert(plugin, "This code must run inside of a plugin")

local toolbar = plugin:CreateToolbar("Luau") :: PluginToolbar
local button =
	toolbar:CreateButton("Language Server Setup", "Toggle Menu", "rbxassetid://11115506617", "Luau LSP") :: PluginToolbarButton

local widgetInfo = DockWidgetPluginGuiInfo.new(
	-- widget info
	Enum.InitialDockState.Float,
	false,
	false,
	300,
	200,
	120,
	70
)

local ConnectAction =
	plugin:CreatePluginAction("Luau LSP Connect", "Connect", "Connects to Luau LSP", "rbxassetid://11115506617", true)

local widget = plugin:CreateDockWidgetPluginGui("Luau Language Server", widgetInfo)
widget.Title = "Luau Language Server"
button.ClickableWhenViewportHidden = true

local port = plugin:GetSetting("Port") or 3667
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
	connected = false
end

local function sendFullDMInfo()
	local tree = encodeInstance(game, filterServices)

	local success, result = pcall(HttpService.RequestAsync, HttpService, {
		Method = "POST",
		Url = string.format("http://localhost:%s/full", port),
		Headers = {
			["Content-Type"] = "application/json",
		},
		Body = HttpService:JSONEncode({
			tree = tree,
		}),
	})

	if not success then
		warn("[Luau Language Server] Connecting to server failed: " .. result)
	elseif not result.Success then
		warn("[Luau Language Server] Sending full DM info failed: " .. result.StatusCode .. ": " .. result.Body)
	end
end

local function watchChanges()
	if connected or port == nil then
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

-- Interface
local theme = settings().Studio.Theme :: StudioTheme
local frame = Instance.new("Frame")
frame.BackgroundColor3 = theme:GetColor(Enum.StudioStyleGuideColor.MainBackground)
frame.Size = UDim2.fromScale(1, 1)
frame.Parent = widget

local listLayout = Instance.new("UIListLayout")
listLayout.Padding = UDim.new(0, 5)
listLayout.SortOrder = Enum.SortOrder.LayoutOrder
listLayout.Parent = frame

local padding = Instance.new("UIPadding")
padding.PaddingTop = UDim.new(0, 5)
padding.PaddingBottom = UDim.new(0, 5)
padding.PaddingLeft = UDim.new(0, 5)
padding.PaddingRight = UDim.new(0, 5)
padding.Parent = frame

local portTextBox = Instance.new("TextBox")
portTextBox.BackgroundColor3 = theme:GetColor(Enum.StudioStyleGuideColor.InputFieldBackground)
portTextBox.BorderColor3 = theme:GetColor(Enum.StudioStyleGuideColor.InputFieldBorder)
portTextBox.TextColor3 = theme:GetColor(Enum.StudioStyleGuideColor.MainText)
portTextBox.PlaceholderColor3 = theme:GetColor(Enum.StudioStyleGuideColor.SubText)
portTextBox.PlaceholderText = "Port"
portTextBox.Text = port
portTextBox.TextSize = 20
portTextBox.Size = UDim2.new(1, 0, 0, 30)
portTextBox.LayoutOrder = 0
portTextBox.Parent = frame
portTextBox.ClearTextOnFocus = false
portTextBox:GetPropertyChangedSignal("Text"):Connect(function()
	portTextBox.Text = portTextBox.Text:gsub("%D+", ""):sub(1, 5)
	port = tonumber(portTextBox.Text)
	plugin:SetSetting("Port", port)
end)

local connectButton = Instance.new("TextButton")
connectButton.BackgroundColor3 = theme:GetColor(Enum.StudioStyleGuideColor.MainButton)
connectButton.BorderColor3 = theme:GetColor(Enum.StudioStyleGuideColor.ButtonBorder)
connectButton.TextColor3 = theme:GetColor(Enum.StudioStyleGuideColor.ButtonText)
connectButton.Text = if connected then "Disconnect" else "Connect"
connectButton.TextSize = 16
connectButton.Size = UDim2.new(1, 0, 0, 25)
connectButton.LayoutOrder = 1
connectButton.Parent = frame
connectButton.Activated:Connect(function()
	if connected then
		print("[Luau Language Server] Disconnecting from DataModel changes")
		cleanup()
	else
		print("[Luau Language Server] Listening for DataModel changes")
		watchChanges()
	end
	connectButton.Text = if connected then "Disconnect" else "Connect"
end)

local corner = Instance.new("UICorner")
corner.CornerRadius = UDim.new(0, 2)
corner.Parent = portTextBox

local corner2 = Instance.new("UICorner")
corner2.CornerRadius = UDim.new(0, 2)
corner2.Parent = connectButton

button.Click:Connect(function()
	widget.Enabled = not widget.Enabled
end)

widget:GetPropertyChangedSignal("Enabled"):Connect(function()
	button:SetActive(widget.Enabled)
end)

ConnectAction.Triggered:Connect(function()
	if connected then
		print("[Luau Language Server] Disconnecting from DataModel changes")
		cleanup()
	else
		print("[Luau Language Server] Listening for DataModel changes")
		watchChanges()
	end
	connectButton.Text = if connected then "Disconnect" else "Connect"
end)
