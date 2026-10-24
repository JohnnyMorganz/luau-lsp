--!strict
local HttpService = game:GetService("HttpService")
local InstanceFileSyncService = game:GetService("InstanceFileSyncService")

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
	--- Whether the plugin is currently connecting to the language server.
	_connecting: boolean,
	--- Incremented when connecting and disconnecting. Work which yields stops when this changes, as it belongs to a
	--- previous connection.
	_generation: number,
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
	--- The session of the full DataModel last sent to the language server, which changes are sent relative to.
	_session: string?,
	--- Whether the language server accepts incremental changes. Otherwise, the full DataModel is sent on every change.
	_supportsChanges: boolean,
	--- The generation of the connection DataModel changes are currently being sent for, if any.
	_syncing: number?,
	--- Whether changes were made while syncing, which need to be sent once the current sync completes.
	_syncRequested: boolean,
	--- Whether the Script Sync status of an instance changed, so file paths need to be refreshed.
	_filePathsChanged: boolean,
	--- Whether any instances are synced with Script Sync, in which case file paths are refreshed on every sync.
	_hasSyncedInstances: boolean,
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
		_connecting = false,
		_generation = 0,
		_address = nil,
		_sendDebounce = nil,
		_autoConnectDebounce = nil,
		_settings = settings,
		_instanceTracker = instanceTracker,
		_silenceServerNotFoundWarning = false,
		_statusChangedConnection = nil,
		_session = nil,
		_supportsChanges = true,
		_syncing = nil,
		_syncRequested = false,
		_filePathsChanged = false,
		_hasSyncedInstances = false,
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
		self:sync()
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

	self._instanceTracker.onChanged:connect(function(): ()
		self:deferredSync()
	end)

	self._statusChangedConnection = InstanceFileSyncService.StatusChanged:Connect(function(instance: Instance): ()
		if not self._instanceTracker:shouldInclude(instance) then
			return
		end

		if self._connected then
			self._filePathsChanged = true
			self:deferredSync()
		else
			local success, instances = pcall(InstanceFileSyncService.GetAllInstances, InstanceFileSyncService)
			if success and #instances > 0 then
				Log.debug("Syncing instances, queueing auto connect")
				self:autoConnect()
			end
		end
	end)
end

--- Returns whether the plugin is currently connected to the language server.
--- @param self -- The LSPManager instance.
--- @return boolean -- True if connected.
function LSPManager.isConnected(self: LSPManager): boolean
	return self._connected
end

--- Returns whether the plugin is currently connecting to the language server.
--- @param self -- The LSPManager instance.
--- @return boolean -- True if connecting.
function LSPManager.isConnecting(self: LSPManager): boolean
	return self._connecting
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
			`{operation} on {self._address} failed. Is the server enabled? Check the settings 'luau-lsp.studioPlugin.enabled' and 'luau-lsp.studioPlugin.port' in your editor`
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
--- Starts tracking changes and sends the initial DataModel tree.
--- @param self -- The LSPManager instance.
--- @return boolean -- True if connection was successful.
function LSPManager.connect(self: LSPManager): boolean
	if self._connected or self._connecting then
		return self._connected
	end

	Log.debug("Connecting to LSP")
	self._connecting = true
	self._generation += 1
	local generation = self._generation

	local ok, success = pcall(self._resync, self, generation)
	if not ok then
		Log.warn(`Failed to send the DataModel: {success}`)
		success = false
	end
	if self._generation ~= generation then
		-- Disconnected while connecting
		return false
	end
	self._connecting = false

	if not success then
		self._instanceTracker:unwatchAll()
		return false
	end

	self._connected = true
	self.onConnectionChanged:fire(true)

	Log.info("Successfully connected")

	-- Send any changes made while connecting
	if self._instanceTracker:hasPendingChanges() then
		self:deferredSync()
	end

	return true
end

--- Disconnects from the language server.
--- Stops tracking changes and cancels any pending updates.
--- @param self -- The LSPManager instance.
function LSPManager.disconnect(self: LSPManager): ()
	if not self._connected and not self._connecting then
		return
	end

	Log.debug("Disconnecting from LSP")
	local wasConnected = self._connected
	self._connected = false
	self._connecting = false
	self._generation += 1
	self._session = nil
	self._supportsChanges = true

	if self._sendDebounce then
		self._sendDebounce:cancel()
	end
	if self._autoConnectDebounce then
		self._autoConnectDebounce:cancel()
	end

	self._instanceTracker:unwatchAll()

	if wasConnected then
		self.onConnectionChanged:fire(false)
		Log.info("Successfully disconnected")
	end
end

--- Get a mapping of instances to their Luau file paths.
--- @param self -- The LSPManager instance.
--- @return { [Instance]: { string } }? -- A mapping of instances to their Luau file paths, or nil if the server failed to respond.
function LSPManager.getFilePaths(self: LSPManager): { [Instance]: { string } }?
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
		local getSuccess, instance = pcall(InstanceFileSyncService.GetSyncedInstance, InstanceFileSyncService, filePath)
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

--- Starts tracking the DataModel from scratch and sends it to the language server.
--- @param self -- The LSPManager instance.
--- @param generation -- The generation of the connection to send the DataModel for.
--- @return boolean -- True if the DataModel was sent successfully.
function LSPManager._resync(self: LSPManager, generation: number): boolean
	local filePaths = self:getFilePaths()
	if self._generation ~= generation then
		return false
	end
	if filePaths == nil then
		-- Server isn't up, don't bother tracking the tree
		-- since it will fail to send anyway
		Log.debug("Server failed to respond, cancelling DataModel send")
		self:disconnect()
		return false
	end
	self._hasSyncedInstances = next(filePaths) ~= nil
	self._filePathsChanged = false

	if not self._instanceTracker:watchAll(filePaths) or self._generation ~= generation then
		-- Stopped tracking while tracking the DataModel
		return false
	end

	return self:sendFullDMInfo(generation)
end

--- Sends the complete tracked DataModel tree to the language server.
--- Called on initial connection, and when the language server needs the full tree.
--- @param self -- The LSPManager instance.
--- @param generation -- The generation of the connection to send the DataModel for.
--- @return boolean -- True if the send was successful.
function LSPManager.sendFullDMInfo(self: LSPManager, generation: number): boolean
	assert(self._address, "LSPManager must be initialized before use")
	Log.debug("Sending full DM info")

	local tree = self._instanceTracker:encodeAll()
	if not tree or self._generation ~= generation then
		return false
	end

	local session = HttpService:GenerateGUID(false)
	tree.Session = session

	local result = ServerEndpoints.full(self._address, tree)
	if self._generation ~= generation then
		return false
	end

	if not result.ok then
		self:_logHttpError("Sending DataModel info", result.error)
		self:disconnect()
		return false
	end

	self._session = session
	Log.debug("Successfully sent full DataModel info")

	return true
end

--- Sends the changes made to tracked instances since the last sync to the language server.
--- @param self -- The LSPManager instance.
--- @param generation -- The generation of the connection to send the changes for.
--- @return boolean -- True if the changes were sent successfully.
function LSPManager._syncChanges(self: LSPManager, generation: number): boolean
	assert(self._address, "LSPManager must be initialized before use")

	local changes = self._instanceTracker:collectChanges()
	if not changes or self._generation ~= generation then
		return false
	end

	if self._instanceTracker:needsRebuild() then
		Log.debug("Included instances moved, resending the full DataModel")
		return self:_resync(generation)
	end

	if self._filePathsChanged or self._hasSyncedInstances then
		self._filePathsChanged = false

		local filePaths = self:getFilePaths()
		if self._generation ~= generation then
			return false
		elseif filePaths == nil then
			self:disconnect()
			return false
		end
		self._hasSyncedInstances = next(filePaths) ~= nil

		for _, change in self._instanceTracker:updateFilePaths(filePaths) do
			table.insert(changes, change)
		end
	end

	if #changes == 0 then
		return true
	end

	if self._supportsChanges and self._session then
		Log.debug(`Sending {#changes} DataModel changes`)

		local result = ServerEndpoints.changes(self._address, self._session, changes)
		if self._generation ~= generation then
			return false
		elseif result.ok then
			return true
		end

		local statusCode = string.match(result.error, "^(%d+):")
		if result.error == "HttpError: ConnectFail" then
			self:_logHttpError("Sending DataModel changes", result.error)
			self:disconnect()
			return false
		elseif statusCode == "404" then
			Log.debug("Language server does not support incremental changes, sending full DataModels instead")
			self._supportsChanges = false
		elseif statusCode == "409" then
			Log.debug(`Language server requested the full DataModel: {result.error}`)
		else
			-- The changes may not have been applied (e.g. the request timed out), so resend everything
			self:_logHttpError("Sending DataModel changes", result.error)
		end
	end

	return self:sendFullDMInfo(generation)
end

--- Sends the changes made to tracked instances since the last sync to the language server.
--- If a sync is already in progress, another sync runs once it completes.
--- @param self -- The LSPManager instance.
function LSPManager.sync(self: LSPManager): ()
	if not self._connected then
		return
	end

	local generation = self._generation
	if self._syncing == generation then
		self._syncRequested = true
		return
	end

	self._syncing = generation
	repeat
		self._syncRequested = false
		local success, result = pcall(self._syncChanges, self, generation)
		if not success and self._generation == generation then
			-- The tracked tree may be inconsistent after an unexpected error, so track everything again
			Log.warn(`Failed to send DataModel changes, resending the full DataModel: {result}`)
			local resyncSuccess, resyncResult = pcall(self._resync, self, generation)
			if not resyncSuccess and self._generation == generation then
				Log.warn(`Failed to send the DataModel: {resyncResult}`)
				self:disconnect()
			end
		end
	until self._generation ~= generation or not self._syncRequested

	if self._syncing == generation then
		self._syncing = nil
	end
end

--- Schedules a deferred DataModel update using debouncing.
--- Multiple rapid changes will be batched into a single update.
--- @param self -- The LSPManager instance.
--- @return thread -- The scheduled task thread.
function LSPManager.deferredSync(self: LSPManager): thread
	assert(self._sendDebounce, "LSPManager must be initialized before use")
	Log.debug("Deferring send of DataModel changes")
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
