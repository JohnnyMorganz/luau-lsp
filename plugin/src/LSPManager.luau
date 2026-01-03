--!strict
local HttpService = game:GetService("HttpService")
local supportsFileSync, InstanceFileSyncService = pcall(game.GetService, game, "InstanceFileSyncService")

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
	--- Debounce controller for auto connect attempts.
	_autoConnectDebounce: Debounce.Debounce?,
	--- Reference to the Settings instance.
	_settings: Settings.Settings,
	--- Reference to the InstanceTracker instance.
	_instanceTracker: InstanceTracker.InstanceTracker,
	--- Whether to silence warnings when the server is not found, used to avoid spam during auto connect attempts.
	_silenceServerNotFoundWarning: boolean,
	--- The connection to the InstanceFileSyncService's StatusChanged event
	_statusChangedConnection: RBXScriptConnection?,
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
		_autoConnectDebounce = nil,
		_settings = settings,
		_instanceTracker = instanceTracker,
		_silenceServerNotFoundWarning = false,
		_statusChangedConnection = nil,
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
	self._autoConnectDebounce = Debounce.new(1, function()
		if self._connected then
			-- Already connected, cancel
			return
		end
		self._silenceServerNotFoundWarning = true
		self:connect()
		self._silenceServerNotFoundWarning = false
	end)

	self._instanceTracker.onChanged:connect(function(_instance: Instance): ()
		-- TODO: Send a delta for this instance instead of resending the full tree
		self:deferredSendFullDMInfo()
	end)

	if supportsFileSync and InstanceFileSyncService then
		-- TODO: When the service type exists, remove the any cast
		self._statusChangedConnection = (InstanceFileSyncService :: any).StatusChanged:Connect(
			function(instance: Instance): ()
				if not self._instanceTracker:shouldInclude(instance) then
					return
				end

				if self._connected then
					self:deferredSendFullDMInfo()
				elseif #(InstanceFileSyncService :: any):GetAllInstances() > 0 then
					Log.debug("Syncing instances, queueing auto connect")
					self:autoConnect()
				end
			end
		)
	end
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
		if self._silenceServerNotFoundWarning then
			return
		end
		Log.warn(
			`{operation} on {self._address} failed. Is the server enabled? Check the settings 'luau-lsp.plugin.enabled' and 'luau-lsp.plugin.port' in your editor`
		)
	else
		Log.warn(`{operation} on {self._address} failed: {result}`)
	end
end

--- Queues an auto connect attempt. Will not warn if the server is not found.
--- @param self -- The LSPManager instance.
function LSPManager.autoConnect(self: LSPManager): ()
	if self._autoConnectDebounce then
		self._autoConnectDebounce:call()
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

	self._instanceTracker:watchAll()

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
	if self._autoConnectDebounce then
		self._autoConnectDebounce:cancel()
	end

	self._instanceTracker:unwatchAll()

	Log.info("Successfully disconnected")
end

--- Get a mapping of instances to their Luau file paths.
--- @param self -- The LSPManager instance.
--- @return { [Instance]: { string } }? -- A mapping of instances to their Luau file paths, or nil if the server failed to respond.
function LSPManager.getFilePaths(self: LSPManager): { [Instance]: { string } }?
	if not supportsFileSync then
		Log.debug("InstanceFileSyncService not supported yet")
		return {}
	end

	assert(self._address, "LSPManager must be initialized before use")
	Log.debug("Getting file paths")

	local result = ServerEndpoints.getFilePaths(self._address)
	if not result.ok then
		-- A 404 indicates the endpoint doesn't exist, likely because we're
		-- connecting to an older language server that predates Script Sync support
		if string.find(result.error, "404") then
			Log.warn(
				"Running against an older version of the language server that does not support Script Sync. Please update luau-lsp for full functionality."
			)
			return {}
		end

		self:_logHttpError("Getting file paths", result.error)
		return nil
	end

	local decodeSuccess, decodeResult = pcall(HttpService.JSONDecode, HttpService, result.value.Body)
	if not decodeSuccess then
		Log.warn(`Failed to decode file paths response: {decodeResult}`)
		return {}
	end

	if typeof(decodeResult) ~= "table" then
		Log.warn(`File paths response is not a table: {typeof(decodeResult)}`)
		return {}
	end

	local filePaths: { string }? = decodeResult["files"]
	if not filePaths then
		Log.warn("No file paths found in response")
		return {}
	end

	local instanceFilePaths = {}

	for _, filePath in filePaths do
		-- TODO: When the service type exists, remove the any cast
		local getSuccess, instance =
			pcall((InstanceFileSyncService :: any).GetSyncedInstance, InstanceFileSyncService, filePath)
		if getSuccess and instance then
			if instanceFilePaths[instance] then
				table.insert(instanceFilePaths[instance], filePath)
			else
				instanceFilePaths[instance] = { filePath }
			end
		end
	end

	return instanceFilePaths
end

--- Sends the complete DataModel tree to the language server.
--- Called on initial connection and when changes are detected.
--- @param self -- The LSPManager instance.
--- @return boolean -- True if the send was successful.
function LSPManager.sendFullDMInfo(self: LSPManager): boolean
	assert(self._address, "LSPManager must be initialized before use")
	Log.debug("Sending full DM info")

	local filePaths = self:getFilePaths()
	if filePaths == nil then
		-- Server isn't up, don't bother encoding the tree
		-- since it will fail to send anyway
		Log.debug("Server failed to respond, cancelling DataModel send")
		self:disconnect()
		return false
	end

	local tree = self._instanceTracker:encodeAll(filePaths)

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
	if self._statusChangedConnection then
		self._statusChangedConnection:Disconnect()
		self._statusChangedConnection = nil
	end
end

return LSPManager
