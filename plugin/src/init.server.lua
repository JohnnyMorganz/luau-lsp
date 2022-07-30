assert(plugin, "no plugin information found")

local toolbar = plugin:CreateToolbar("Luau Language Server") :: PluginToolbar
local button =
	toolbar:CreateButton("Connect to Server", "Connect to Server", "Connect to Server") :: PluginToolbarButton

local widgetInfo = DockWidgetPluginGuiInfo.new(
	-- Widget settings
	Enum.InitialDockState.Right,
	false,
	false,
	200,
	300,
	150,
	150
)

local widget = plugin:CreateDockWidgetPluginGui("ConnectServer", widgetInfo)

button.Click:Connect(function()
	widget.Enabled = not widget.Enabled
end)
