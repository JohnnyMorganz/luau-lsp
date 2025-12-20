--- Configuration for the Luau Language Server
--- https://devforum.roblox.com/t/luau-language-server-for-external-editors/2185389
--- Make sure that `luau-lsp.plugin.enabled` is set to `true` in VSCode for the plugin to connect

return {
	--- The host to connect to the language server.
	host = "http://localhost",

	--- The port to connect to the language server.
	--- In VSCode, this is configured using `luau-lsp.plugin.port`
	--- Note, this port is *different* to your Rojo port
	port = 3667,

	--- Whether the plugin should automatically connect to the language server when Studio starts up
	startAutomatically = false,

	--- A list of Instances to track for changes.
	--- When any descendants of these instances change, an update event is sent to the language server
	include = {
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

	--- The log level to use for the language server.
	--- Valid values are "DEBUG", "INFO", "WARN", "ERROR"
	logLevel = "INFO",
}
