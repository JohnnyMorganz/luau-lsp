--!strict

--- Manages the connection to the Luau Language Server.
--- Handles connecting, disconnecting, and sending DataModel updates.

local Debounce = require(script.Parent.Utils.Debounce)
local InstanceTracker = require(script.Parent.InstanceTracker)
local Log = require(script.Parent.Utils.Log)
local ServerEndpoints = require(script.Parent.ServerEndpoints)
local Settings = require(script.Parent.Settings)
local Signal = require(script.Parent.Utils.Signal)

local LSPManager = {}
LSPManager.__index = LSPManager

--- Internal data structure for an LSPManager instance.
type LSPManagerData = {
	--- Whether the plugin is currently connected to the language server.
	_connected: boolean,
	--- The full address of the language server (e.g., "http://localhost:3667").
	_address: string?,
	--- Debounce controller for batching DataModel updates.
	_sendDebounce: Debounce.Debounce?,
	--- Reference to the Settings instance.
	_settings: Settings.Settings,
	--- Reference to the InstanceTracker instance.
	_instanceTracker: InstanceTracker.InstanceTracker,
	--- Signal fired when connection state changes.
	onConnectionChanged: Signal.Signal<boolean>,
}

--- Manages the connection to the Luau Language Server.
export type LSPManager = typeof(setmetatable({} :: LSPManagerData, LSPManager))

--- Creates a new LSPManager instance.
--- @param settings -- The Settings instance for configuration.
--- @param instanceTracker -- The InstanceTracker instance for change tracking.
--- @return LSPManager -- A new LSPManager ready for initialization.
function LSPManager.new(settings: Settings.Settings, instanceTracker: InstanceTracker.InstanceTracker): LSPManager
	local self = {
		_connected = false,
		_address = nil,
		_sendDebounce = nil,
		_settings = settings,
		_instanceTracker = instanceTracker,
		onConnectionChanged = Signal.new() :: Signal.Signal<boolean>,
	}
	return setmetatable(self, LSPManager)
end

--- Initializes the LSP manager with settings and change tracking.
--- Must be called before any other LSPManager functions.
--- @param self -- The LSPManager instance.
function LSPManager.init(self: LSPManager): ()
	self:setAddress(self._settings:get("host"), self._settings:get("port"))
	self._sendDebounce = Debounce.new(0.5, function()
		if not self._connected then
			return
		end
		self:sendFullDMInfo()
	end)

	self._instanceTracker.onChanged:connect(function(_instance: Instance): ()
		-- TODO: Send a delta for this instance instead of resending the full tree
		self:deferredSendFullDMInfo()
	end)
end

--- Returns whether the plugin is currently connected to the language server.
--- @param self -- The LSPManager instance.
--- @return boolean -- True if connected.
function LSPManager.isConnected(self: LSPManager): boolean
	return self._connected
end

--- Sets the language server address from host and port components.
--- Automatically adds the http:// protocol if not present.
--- @param self -- The LSPManager instance.
--- @param host -- The server hostname (e.g., "localhost" or "http://localhost").
--- @param port -- The server port number.
function LSPManager.setAddress(self: LSPManager, host: string, port: number): ()
	-- Ensure host has protocol
	if not host:match("^https?://") then
		host = "http://" .. host
	end
	-- Remove trailing slash
	host = host:gsub("/$", "")

	Log.debug(`Setting LSP address to {host}:{port}`)
	self._address = `{host}:{port}`
end

--- Internal function to log HTTP errors with helpful context.
--- @param self -- The LSPManager instance.
--- @param operation -- Description of the operation that failed.
--- @param result -- The error result from the HTTP request.
function LSPManager._logHttpError(self: LSPManager, operation: string, result: any): ()
	if result == "HttpError: ConnectFail" then
		Log.warn(
			`{operation} on {self._address} failed. Is the server enabled? Check the settings 'luau-lsp.plugin.enabled' and 'luau-lsp.plugin.port' in your editor`
		)
	else
		Log.warn(`{operation} on {self._address} failed: {result}`)
	end
end

--- Establishes a connection to the language server.
--- Sends the initial DataModel tree and starts tracking changes.
--- @param self -- The LSPManager instance.
--- @return boolean -- True if connection was successful.
function LSPManager.connect(self: LSPManager): boolean
	if self._connected then
		return true
	end

	Log.debug("Connecting to LSP")

	local success = self:sendFullDMInfo()
	if not success then
		return false
	end

	local includeList = self._settings:get("include")
	self._instanceTracker:watchAll(includeList)

	self._connected = true
	self.onConnectionChanged:fire(true)

	Log.info("Successfully connected")

	return true
end

--- Disconnects from the language server.
--- Stops tracking changes and cancels any pending updates.
--- @param self -- The LSPManager instance.
function LSPManager.disconnect(self: LSPManager): ()
	if not self._connected then
		return
	end

	Log.debug("Disconnecting from LSP")
	self._connected = false
	self.onConnectionChanged:fire(false)

	if self._sendDebounce then
		self._sendDebounce:cancel()
	end

	self._instanceTracker:unwatchAll()

	Log.info("Successfully disconnected")
end

--- Sends the complete DataModel tree to the language server.
--- Called on initial connection and when changes are detected.
--- @param self -- The LSPManager instance.
--- @return boolean -- True if the send was successful.
function LSPManager.sendFullDMInfo(self: LSPManager): boolean
	assert(self._address, "LSPManager must be initialized before use")
	Log.debug("Sending full DM info")

	local tree = self._instanceTracker:encodeAll()

	local result = ServerEndpoints.full(self._address, tree)

	if not result.ok then
		self:_logHttpError("Sending DataModel info", result.error)
		self:disconnect()
		return false
	end

	Log.debug("Successfully sent full DataModel info")

	return true
end

--- Schedules a deferred DataModel update using debouncing.
--- Multiple rapid changes will be batched into a single update.
--- @param self -- The LSPManager instance.
--- @return thread -- The scheduled task thread.
function LSPManager.deferredSendFullDMInfo(self: LSPManager): thread
	assert(self._sendDebounce, "LSPManager must be initialized before use")
	Log.debug("Deferring send of DataModel info")
	return self._sendDebounce:call()
end

--- Clears the DataModel information from the language server.
--- @param self -- The LSPManager instance.
--- @return boolean -- True if the clear was successful.
function LSPManager.clearDMInfo(self: LSPManager): boolean
	assert(self._address, "LSPManager must be initialized before use")
	Log.debug("Clearing DM info")

	local result = ServerEndpoints.clear(self._address)

	if not result.ok then
		self:_logHttpError("Clearing DataModel info", result.error)
		return false
	end

	Log.debug("Successfully cleared DataModel info")

	return true
end

--- Cleans up the LSP manager.
--- Disconnects and destroys all signals.
--- Call this when the plugin is unloading.
--- @param self -- The LSPManager instance.
function LSPManager.unload(self: LSPManager): ()
	self:disconnect()
	self.onConnectionChanged:destroy()
	self._instanceTracker:unload()
end

return LSPManager
