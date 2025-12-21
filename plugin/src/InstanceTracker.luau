--!strict

--- Tracks changes to Roblox Instances and notifies listeners with filtering based on include paths.
--- Monitors name and parent property changes, as well as new descendants.
--- Encodes Roblox Instances into a serializable format for the language server.

local Log = require(script.Parent.Utils.Log)
local Signal = require(script.Parent.Utils.Signal)
local types = require(script.Parent.types)

local InstanceTracker = {}
InstanceTracker.__index = InstanceTracker

--- Internal data structure for an InstanceTracker instance.
type InstanceTrackerData = {
	--- Map of tracked instances to their associated connections.
	_connections: { [Instance]: { RBXScriptConnection } },
	--- Function that returns the list of instances to include.
	_getIncludeList: () -> { Instance },
	--- Signal fired when any tracked instance changes.
	onChanged: Signal.Signal<Instance>,
}

--- Tracks changes to Roblox Instances and notifies listeners.
export type InstanceTracker = typeof(setmetatable({} :: InstanceTrackerData, InstanceTracker))

--- Creates a new InstanceTracker instance.
--- @param getIncludeList -- Function that returns the list of instances to include.
--- @return InstanceTracker -- A new InstanceTracker ready to watch instances.
function InstanceTracker.new(getIncludeList: () -> { Instance }): InstanceTracker
	local self = {
		_connections = {},
		_getIncludeList = getIncludeList,
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

		if instance.Parent == nil or not self:shouldInclude(instance) then
			-- Instance is no longer tracked, stop tracking it
			self:untrack(instance)
		end
	end)
	self._connections[instance] = { nameConnection, parentConnection }
end

--- Stops tracking changes to a specific Instance and its children.
--- Disconnects all associated event connections.
--- @param self -- The InstanceTracker instance.
--- @param instance -- The Instance to stop tracking.
function InstanceTracker.untrack(self: InstanceTracker, instance: Instance): ()
	Log.debug(`Un-tracking changes for {instance:GetFullName()}`)

	local connections = self._connections[instance]
	if not connections then
		-- We are already not tracking this instance
		return
	end

	for _, child in instance:GetChildren() do
		self:untrack(child)
	end

	for _, connection in connections do
		connection:Disconnect()
	end
	self._connections[instance] = nil
end

--- Starts tracking all instances in the provided include list.
--- Also watches for new descendants being added to the DataModel.
--- @param self -- The InstanceTracker instance.
function InstanceTracker.watchAll(self: InstanceTracker): ()
	Log.debug("Watching changes")

	-- Track initial instances
	for _, instance in self._getIncludeList() do
		self:track(instance)

		for _, descendant in instance:GetDescendants() do
			self:track(descendant)
		end
	end

	-- Track new instances
	self._connections[game] = {
		game.DescendantAdded:Connect(function(instance)
			if self:shouldInclude(instance) then
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

--- Determines whether an Instance should be included in the encoded tree.
--- An instance is included if it is within, contains, or is one of the
--- provided include paths.
--- @param instance -- The Instance to check.
--- @param includeList? -- The list of instances to include.
--- @return boolean -- True if the instance should be included.
function InstanceTracker.shouldInclude(self: InstanceTracker, instance: Instance, includeList: { Instance }?): boolean
	-- Ignore instances that are not part of any tree
	if instance.Parent == nil then
		return false
	end

	-- Check if this instance is within, contains, or is an included tree
	for _, includedInstance in (includeList or self._getIncludeList()) :: { Instance } do
		if
			instance == includedInstance
			or instance:IsDescendantOf(includedInstance)
			or instance:IsAncestorOf(includedInstance)
		then
			return true
		end
	end

	-- Ignore all instances that are not explicitly included
	return false
end

--- Recursively encodes an Instance and its children into a serializable table.
--- Only includes children that pass the `shouldInclude` filter.
--- @param self -- The InstanceTracker instance.
--- @param instance -- The Instance to encode.
--- @param includeList -- The list of instances to include.
--- @param instanceFilePaths -- A mapping of instances to their Luau file paths.
--- @return types.EncodedInstance -- The encoded representation.
function InstanceTracker.encode(
	self: InstanceTracker,
	instance: Instance,
	includeList: { Instance },
	instanceFilePaths: { [Instance]: { string } }
): types.EncodedInstance
	Log.debug(`Encoding {instance:GetFullName()}`)

	local encoded: types.EncodedInstance = {
		Name = instance.Name,
		ClassName = instance.ClassName,
		FilePaths = instanceFilePaths[instance] or {},
		Children = {},
	}

	for _, child in instance:GetChildren() do
		if not self:shouldInclude(child, includeList) then
			continue
		end

		table.insert(encoded.Children, self:encode(child, includeList, instanceFilePaths))
	end

	return encoded
end

--- Utility function to encode the entire DataModel into a serializable table.
--- @param self -- The InstanceTracker instance.
--- @param instanceFilePaths -- A mapping of instances to their Luau file paths.
--- @return types.EncodedInstance -- The encoded representation of the DataModel.
function InstanceTracker.encodeAll(
	self: InstanceTracker,
	instanceFilePaths: { [Instance]: { string } }
): types.EncodedInstance
	return self:encode(game, self._getIncludeList(), instanceFilePaths)
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
