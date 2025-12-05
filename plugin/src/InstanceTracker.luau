--!strict

--- Tracks changes to Roblox Instances and notifies listeners.
--- Monitors name and parent property changes, as well as new descendants.

local Log = require(script.Parent.Log)
local Signal = require(script.Parent.Signal)

local InstanceTracker = {}
InstanceTracker.__index = InstanceTracker

--- Internal data structure for an InstanceTracker instance.
type InstanceTrackerData = {
	--- Map of tracked instances to their associated connections.
	_connections: { [Instance]: { RBXScriptConnection } },
	--- Function to check if an instance should be included.
	_includeChecker: (Instance) -> boolean,
	--- Signal fired when any tracked instance changes.
	onChanged: Signal.Signal<Instance>,
}

--- Tracks changes to Roblox Instances and notifies listeners.
export type InstanceTracker = typeof(setmetatable({} :: InstanceTrackerData, InstanceTracker))

--- Creates a new InstanceTracker instance.
--- @param includeChecker -- Function that returns true if an instance should be tracked.
--- @return InstanceTracker -- A new InstanceTracker ready to watch instances.
function InstanceTracker.new(includeChecker: (Instance) -> boolean): InstanceTracker
	local self = {
		_connections = {},
		_includeChecker = includeChecker,
		onChanged = Signal.new() :: Signal.Signal<Instance>,
	}
	return setmetatable(self, InstanceTracker)
end

--- Starts tracking changes to a specific Instance.
--- Monitors the Name and Parent properties for changes.
--- @param self -- The InstanceTracker instance.
--- @param instance -- The Instance to track.
function InstanceTracker.track(self: InstanceTracker, instance: Instance): ()
	local nameConnection = instance:GetPropertyChangedSignal("Name"):Connect(function()
		Log.debug(`Instance {instance:GetFullName()} name changed`)
		self.onChanged:fire(instance)
	end)
	local parentConnection = instance:GetPropertyChangedSignal("Parent"):Connect(function()
		Log.debug(`Instance {instance:GetFullName()} parent changed`)
		self.onChanged:fire(instance)

		if instance.Parent == nil or not self._includeChecker(instance) then
			-- Instance is no longer tracked, stop tracking it and its descendants
			self:untrack(instance)
			for _, descendant in instance:GetDescendants() do
				self:untrack(descendant)
			end
		end
	end)
	self._connections[instance] = { nameConnection, parentConnection }
end

--- Stops tracking changes to a specific Instance.
--- Disconnects all associated event connections.
--- @param self -- The InstanceTracker instance.
--- @param instance -- The Instance to stop tracking.
function InstanceTracker.untrack(self: InstanceTracker, instance: Instance): ()
	Log.debug(`Un-tracking changes for {instance:GetFullName()}`)

	local connections = self._connections[instance]
	if not connections then
		return
	end

	for _, connection in connections do
		connection:Disconnect()
	end
	self._connections[instance] = nil
end

--- Starts tracking all instances in the provided include list.
--- Also watches for new descendants being added to the DataModel.
--- @param self -- The InstanceTracker instance.
--- @param includeList -- The list of root instances to track.
function InstanceTracker.watchAll(self: InstanceTracker, includeList: { Instance }): ()
	Log.debug("Watching changes")

	-- Track initial instances
	for _, instance in includeList do
		self:track(instance)

		for _, descendant in instance:GetDescendants() do
			self:track(descendant)
		end
	end

	-- Track new instances
	self._connections[game] = {
		game.DescendantAdded:Connect(function(instance)
			if self._includeChecker(instance) then
				self:track(instance)
				self.onChanged:fire(instance)
			end
		end),
	}
end

--- Stops tracking all instances and clears all connections.
--- Call this when disconnecting from the language server.
--- @param self -- The InstanceTracker instance.
function InstanceTracker.unwatchAll(self: InstanceTracker): ()
	-- Collect instances first to avoid modifying table while iterating
	local instances = {}
	for instance in self._connections do
		table.insert(instances, instance)
	end
	for _, instance in instances do
		self:untrack(instance)
	end
	self._connections = {}
end

--- Cleans up the InstanceTracker.
--- Stops tracking all instances and destroys the signal.
--- Call this when the plugin is unloading.
--- @param self -- The InstanceTracker instance.
function InstanceTracker.unload(self: InstanceTracker): ()
	self:unwatchAll()
	self.onChanged:destroy()
end

return InstanceTracker
