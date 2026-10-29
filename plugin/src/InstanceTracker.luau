--!strict

--- Tracks the included Roblox Instances and keeps a mirror of their tree, so that the language server can be
--- updated with incremental changes instead of re-encoding the whole DataModel after every change.
---
--- Instance events only mark instances as dirty, which keeps them cheap. `collectChanges` then reconciles the dirty
--- instances against the live DataModel and returns the changes made since it was last called. Reconciling against
--- the live DataModel, rather than replaying events, keeps the mirror correct with deferred events and rapid changes.
---
--- Work proportional to the size of the DataModel (initial tracking, tracking a large inserted subtree) is spread
--- over multiple frames to avoid freezing Studio.

local FrameBudget = require(script.Parent.Utils.FrameBudget)
local Log = require(script.Parent.Utils.Log)
local Signal = require(script.Parent.Utils.Signal)
local types = require(script.Parent.types)

--- How long tracking work may run each frame before yielding to Studio.
local FRAME_BUDGET_SECONDS = 0.008

local InstanceTracker = {}
InstanceTracker.__index = InstanceTracker

--- A tracked instance in the mirror.
type Node = {
	--- Identifies the instance in changes sent to the language server.
	id: number,
	name: string,
	className: string,
	--- The parent of the instance in the mirror. Nil for the root.
	parent: Instance?,
	--- The tracked children of the instance.
	children: { [Instance]: true }?,
	--- Orders the instance among its siblings, which are encoded in the same order as `GetChildren`: instances are
	--- added to the end of their parent's children, including when they are moved.
	order: number,
	--- Whether the instance is, or is a descendant of, an included instance. Otherwise, it is an ancestor of an
	--- included instance, and only its children leading to included instances are tracked.
	fullyIncluded: boolean,
	filePaths: { string }?,
	nameConnection: RBXScriptConnection,
	parentConnection: RBXScriptConnection?,
}

--- The mirror and connections of a tracking session, from `watchAll` until `unwatchAll`.
--- Work that yields keeps a reference to its session, and stops if the session is no longer active.
type Session = {
	active: boolean,
	nodes: { [Instance]: Node },
	--- Instances that may have changed since changes were last collected.
	dirty: { [Instance]: true },
	--- Whether any instances were marked dirty since changes were last collected.
	hasDirty: boolean,
	--- The order given to the next tracked or moved instance.
	nextOrder: number,
	--- Whether an included instance or one of its ancestors moved, which requires tracking everything again.
	needsRebuild: boolean,
	includeSet: { [Instance]: true },
	--- The latest mapping of instances to their Luau file paths.
	filePaths: { [Instance]: { string } },
	--- Tracked instances which have file paths.
	withFilePaths: { [Instance]: true },
	--- Connections to the root and the included instances.
	connections: { RBXScriptConnection },
	--- Connections of instances which stopped being tracked, which are only disconnected when changes are next
	--- collected: events fired before then may not have been handled yet, and must still mark the instance dirty.
	droppedConnections: { RBXScriptConnection },
}

--- Internal data structure for an InstanceTracker instance.
type InstanceTrackerData = {
	--- The root of the tracked tree.
	_root: Instance,
	--- Function that returns the list of instances to include.
	_getIncludeList: () -> { Instance },
	--- The current tracking session, if tracking.
	_session: Session?,
	--- The Id given to the next tracked instance.
	_nextId: number,
	--- Signal fired when tracked instances changed and there are changes to collect.
	onChanged: Signal.Signal<()>,
}

--- Tracks changes to Roblox Instances and notifies listeners.
export type InstanceTracker = typeof(setmetatable({} :: InstanceTrackerData, InstanceTracker))

--- Creates a new InstanceTracker instance.
--- @param getIncludeList -- Function that returns the list of instances to include.
--- @param root? -- The root of the tracked tree. Defaults to the DataModel.
--- @return InstanceTracker -- A new InstanceTracker ready to watch instances.
function InstanceTracker.new(getIncludeList: () -> { Instance }, root: Instance?): InstanceTracker
	local self = {
		_root = root or game,
		_getIncludeList = getIncludeList,
		_session = nil,
		_nextId = 1,
		onChanged = Signal.new() :: Signal.Signal<()>,
	}
	return setmetatable(self, InstanceTracker)
end

--- Replaces invalid UTF-8 in an instance name, as it cannot be encoded to JSON.
--- @param name -- The instance name.
--- @return string -- The name, with each invalid UTF-8 byte replaced by U+FFFD.
local function encodeName(name: string): string
	if utf8.len(name) then
		return name
	end

	local parts = {}
	local position = 1
	while true do
		local length, invalidPosition = utf8.len(name, position)
		if length or not invalidPosition then
			table.insert(parts, string.sub(name, position))
			break
		end
		table.insert(parts, string.sub(name, position, invalidPosition - 1))
		table.insert(parts, utf8.char(0xFFFD))
		position = invalidPosition + 1
	end
	return table.concat(parts)
end

--- Warns if the name of an instance is not valid UTF-8.
--- @param instance -- The instance to check.
--- @param name -- The name of the instance.
local function warnIfInvalidName(instance: Instance, name: string): ()
	if not utf8.len(name) then
		Log.warn(
			`The name of instance "{instance:GetFullName()}" contains invalid UTF-8, which was replaced by U+FFFD.`
		)
	end
end

--- Marks an instance as possibly changed, notifying listeners if there were no pending changes.
--- @param self -- The InstanceTracker instance.
--- @param session -- The session the instance belongs to.
--- @param instance -- The instance that may have changed.
function InstanceTracker._markDirty(self: InstanceTracker, session: Session, instance: Instance): ()
	if not session.active then
		return
	end

	session.dirty[instance] = true
	if not session.hasDirty then
		session.hasDirty = true
		self.onChanged:fire()
	end
end

--- Starts tracking an instance under its tracked parent.
--- @param self -- The InstanceTracker instance.
--- @param session -- The session to track the instance in.
--- @param instance -- The instance to track.
--- @param parent -- The tracked parent of the instance, or nil for the root.
--- @param fullyIncluded -- Whether the instance is, or is a descendant of, an included instance.
--- @return Node -- The node tracking the instance.
function InstanceTracker._createNode(
	self: InstanceTracker,
	session: Session,
	instance: Instance,
	parent: Instance?,
	fullyIncluded: boolean
): Node
	local name = instance.Name
	warnIfInvalidName(instance, name)

	local function onChanged()
		self:_markDirty(session, instance)
	end

	-- `children` and `filePaths` are only set when needed: most nodes have neither, and listing nil fields in the
	-- constructor would allocate space for them in every node
	local node: Node = {
		id = self._nextId,
		name = name,
		className = instance.ClassName,
		parent = parent,
		order = session.nextOrder,
		fullyIncluded = fullyIncluded,
		nameConnection = instance:GetPropertyChangedSignal("Name"):Connect(onChanged),
		parentConnection = if parent then instance:GetPropertyChangedSignal("Parent"):Connect(onChanged) else nil,
	}
	self._nextId += 1
	session.nextOrder += 1

	session.nodes[instance] = node
	local filePaths = session.filePaths[instance]
	if filePaths then
		node.filePaths = filePaths
		session.withFilePaths[instance] = true
	end
	if parent then
		local parentNode = session.nodes[parent]
		local siblings = parentNode.children or {}
		parentNode.children = siblings
		siblings[instance] = true
	end

	return node
end

--- Stops tracking an instance and its tracked descendants.
--- @param session -- The session the instance is tracked in.
--- @param instance -- The instance to stop tracking.
local function dropSubtree(session: Session, instance: Instance): ()
	local node = session.nodes[instance]
	session.nodes[instance] = nil
	session.withFilePaths[instance] = nil

	table.insert(session.droppedConnections, node.nameConnection)
	if node.parentConnection then
		table.insert(session.droppedConnections, node.parentConnection)
	end

	if node.children then
		for child in node.children do
			dropSubtree(session, child)
		end
	end
end

--- Stops tracking an instance and its tracked descendants, removing it from its tracked parent.
--- @param session -- The session the instance is tracked in.
--- @param instance -- The instance to stop tracking.
local function dropNode(session: Session, instance: Instance): ()
	local parent = session.nodes[instance].parent
	local parentNode = parent and session.nodes[parent]
	if parentNode and parentNode.children then
		parentNode.children[instance] = nil
	end

	dropSubtree(session, instance)
end

--- Moves a tracked instance to another tracked parent.
--- @param session -- The session the instance is tracked in.
--- @param instance -- The instance to move.
--- @param node -- The node tracking the instance.
--- @param parent -- The new tracked parent of the instance.
local function moveNode(session: Session, instance: Instance, node: Node, parent: Instance): ()
	local oldParentNode = node.parent and session.nodes[node.parent]
	if oldParentNode and oldParentNode.children then
		oldParentNode.children[instance] = nil
	end

	node.parent = parent
	node.order = session.nextOrder
	session.nextOrder += 1
	local parentNode = session.nodes[parent]
	local siblings = parentNode.children or {}
	parentNode.children = siblings
	siblings[instance] = true
end

--- Determines whether a child of a tracked instance should be tracked.
--- @param session -- The session the parent is tracked in.
--- @param child -- The child to check.
--- @param parentNode -- The node tracking the parent of the child.
--- @return boolean -- True if the child should be tracked.
--- @return boolean -- True if the child is, or is a descendant of, an included instance.
local function includeChild(session: Session, child: Instance, parentNode: Node): (boolean, boolean)
	if parentNode.fullyIncluded or session.includeSet[child] then
		return true, true
	end

	-- The parent is an ancestor of an included instance: only track children leading to included instances
	for includedInstance in session.includeSet do
		if child:IsAncestorOf(includedInstance) then
			return true, false
		end
	end

	return false, false
end

--- Starts tracking an instance and its descendants. Yields when the frame budget is used up.
--- @param self -- The InstanceTracker instance.
--- @param session -- The session to track the instances in.
--- @param instance -- The instance to track.
--- @param parent -- The tracked parent of the instance, or nil for the root.
--- @param fullyIncluded -- Whether the instance is, or is a descendant of, an included instance.
--- @param budget -- The frame budget to yield with.
--- @return Node? -- The node tracking the instance, or nil if the session stopped while tracking.
function InstanceTracker._buildSubtree(
	self: InstanceTracker,
	session: Session,
	instance: Instance,
	parent: Instance?,
	fullyIncluded: boolean,
	budget: FrameBudget.FrameBudget
): Node?
	local node = self:_createNode(session, instance, parent, fullyIncluded)

	if fullyIncluded then
		-- Track a snapshot of the descendants. Descendants may move while this yields, which is not always
		-- notified: descendants which are not under an instance tracked by this build are reconciled afterwards
		local built: { [Instance]: true } = { [instance] = true }
		for _, descendant in instance:GetDescendants() do
			if budget:step() and not session.active then
				return nil
			end

			local descendantParent = descendant.Parent
			local tracked = session.nodes[descendant] ~= nil
			if descendantParent == nil then
				continue
			elseif built[descendantParent] and not tracked then
				self:_createNode(session, descendant, descendantParent, true)
				built[descendant] = true
			elseif built[descendantParent] or not tracked then
				self:_markDirty(session, descendant)
			end
		end
	else
		-- Ancestors of included instances have few tracked children, check them directly
		for _, child in instance:GetChildren() do
			local included, childFullyIncluded = includeChild(session, child, node)
			if included and not session.nodes[child] and child.Parent == instance then
				if not self:_buildSubtree(session, child, instance, childFullyIncluded, budget) then
					return nil
				end
			end
		end
	end
	return node
end

--- Recursively encodes a tracked instance and its tracked children into a serializable table, without traversing
--- the DataModel. Empty `FilePaths` and `Children` are omitted to reduce the size of the encoded tree.
--- Yields when the frame budget is used up.
--- @param self -- The InstanceTracker instance.
--- @param session -- The session the instance is tracked in.
--- @param instance -- The instance to encode.
--- @param budget -- The frame budget to yield with.
--- @return types.EncodedInstance -- The encoded representation.
function InstanceTracker._encode(
	self: InstanceTracker,
	session: Session,
	instance: Instance,
	budget: FrameBudget.FrameBudget
): types.EncodedInstance
	budget:step()

	local node = session.nodes[instance]
	local encoded: types.EncodedInstance = {
		Id = node.id,
		Name = encodeName(node.name),
		ClassName = node.className,
	}
	if node.filePaths and #node.filePaths > 0 then
		encoded.FilePaths = node.filePaths
	end

	if node.children and next(node.children) then
		local childInstances = {}
		for child in node.children do
			table.insert(childInstances, child)
		end
		if #childInstances > 1 then
			table.sort(childInstances, function(a, b)
				return session.nodes[a].order < session.nodes[b].order
			end)
		end

		local children = table.create(#childInstances)
		for index, child in childInstances do
			children[index] = self:_encode(session, child, budget)
		end
		encoded.Children = children
	end

	return encoded
end

--- Makes sure a dirty instance is tracked under its current parent if it should be, tracking its parent first if
--- needed. Changes are produced in an order where every instance is only ever moved under an instance which is not
--- its descendant. Yields when the frame budget is used up.
--- @param self -- The InstanceTracker instance.
--- @param session -- The session to track the instance in.
--- @param instance -- The instance to attach.
--- @param detached -- Tracked instances which are no longer under their tracked parent.
--- @param visited -- Instances which have already been attached, or could not be.
--- @param changes -- The list to add the resulting changes to.
--- @param budget -- The frame budget to yield with.
--- @return Node? -- The node tracking the instance, or nil if it should not be tracked.
function InstanceTracker._attach(
	self: InstanceTracker,
	session: Session,
	instance: Instance,
	detached: { [Instance]: true },
	visited: { [Instance]: true },
	changes: { types.Change },
	budget: FrameBudget.FrameBudget
): Node?
	local node = session.nodes[instance]
	if node and not detached[instance] then
		-- The instance is tracked under its current parent, but a tracked ancestor may have moved. Attach the
		-- closest one first, so that the tracked ancestors of the instance are its actual ancestors
		local ancestor = if next(detached) then node.parent else nil
		while ancestor do
			if detached[ancestor] then
				if not self:_attach(session, ancestor, detached, visited, changes, budget) then
					return nil
				end
				break
			end
			ancestor = session.nodes[ancestor].parent
		end
		return node
	elseif visited[instance] then
		return nil
	end
	visited[instance] = true

	local parent = instance.Parent
	if parent == nil then
		return nil
	end

	local parentNode = self:_attach(session, parent, detached, visited, changes, budget)
	if not parentNode or not session.active then
		return nil
	end

	-- Tracking the parent may have tracked this instance too
	node = session.nodes[instance]
	if node and not detached[instance] then
		return node
	elseif instance.Parent ~= parent then
		-- Moved while tracking the parent, which is not notified for instances that are not tracked yet
		self:_markDirty(session, instance)
		return nil
	end

	local included, fullyIncluded = includeChild(session, instance, parentNode)
	if not included then
		return nil
	end

	if node then
		detached[instance] = nil
		moveNode(session, instance, node, parent)
		table.insert(
			changes,
			{ Type = "Update", Id = node.id, Parent = parentNode.id, Name = encodeName(node.name) } :: types.Change
		)
		return node
	end

	local newNode = self:_buildSubtree(session, instance, parent, fullyIncluded, budget)
	if newNode then
		table.insert(
			changes,
			{ Type = "Add", Parent = parentNode.id, Instance = self:_encode(session, instance, budget) } :: types.Change
		)
	end
	return newNode
end

--- Starts tracking all instances in the provided include list, stopping any previous tracking.
--- Yields while tracking, spreading the work over multiple frames.
--- @param self -- The InstanceTracker instance.
--- @param instanceFilePaths -- A mapping of instances to their Luau file paths.
--- @return boolean -- False if tracking was stopped before it finished.
function InstanceTracker.watchAll(self: InstanceTracker, instanceFilePaths: { [Instance]: { string } }): boolean
	self:unwatchAll()
	Log.debug("Watching changes")

	local includeSet: { [Instance]: true } = {}
	for _, instance in self._getIncludeList() do
		includeSet[instance] = true
	end

	local session: Session = {
		active = true,
		nodes = {},
		dirty = {},
		hasDirty = false,
		nextOrder = 1,
		needsRebuild = false,
		includeSet = includeSet,
		filePaths = instanceFilePaths,
		withFilePaths = {},
		connections = {},
		droppedConnections = {},
	}
	self._session = session

	-- Listen for changes before tracking, so that changes made while tracking are not missed.
	-- Moving an included instance or one of its ancestors changes which instances are included, so everything must be
	-- tracked again. Removals are detected from the root as well as from the removed instance's own connections,
	-- since destroying an instance also disconnects its connections
	local function onIncludedAncestryChanged(instance: Instance)
		session.needsRebuild = true
		self:_markDirty(session, instance)
	end
	table.insert(
		session.connections,
		self._root.DescendantRemoving:Connect(function(descendant)
			local node = session.nodes[descendant]
			if includeSet[descendant] or (node and not node.fullyIncluded) then
				onIncludedAncestryChanged(descendant)
			elseif node then
				self:_markDirty(session, descendant)
			end
		end)
	)
	for instance in includeSet do
		table.insert(
			session.connections,
			instance.AncestryChanged:Connect(function()
				onIncludedAncestryChanged(instance)
			end)
		)
		table.insert(
			session.connections,
			instance.DescendantAdded:Connect(function(descendant)
				-- Every added instance is reconciled, rather than only the topmost one, as descendants of added
				-- instances can move before they are tracked without firing any event that is listened to
				self:_markDirty(session, descendant)
			end)
		)
	end

	local startTime = os.clock()
	local budget = FrameBudget.new(FRAME_BUDGET_SECONDS)
	if not self:_buildSubtree(session, self._root, nil, includeSet[self._root] == true, budget) then
		return false
	end
	Log.debug(`Tracked instances in {math.round((os.clock() - startTime) * 1000)}ms`)

	return session.active
end

--- Stops tracking all instances and clears all connections.
--- Call this when disconnecting from the language server.
--- @param self -- The InstanceTracker instance.
function InstanceTracker.unwatchAll(self: InstanceTracker): ()
	local session = self._session
	if not session then
		return
	end

	self._session = nil
	session.active = false
	for _, connection in session.connections do
		connection:Disconnect()
	end
	for _, connection in session.droppedConnections do
		connection:Disconnect()
	end

	-- Disconnecting every tracked instance can take a while in large DataModels, so spread it over multiple frames
	task.spawn(function()
		local budget = FrameBudget.new(FRAME_BUDGET_SECONDS)
		for _, node in session.nodes do
			budget:step()
			node.nameConnection:Disconnect()
			if node.parentConnection then
				node.parentConnection:Disconnect()
			end
		end
	end)
end

--- Returns whether there are changes waiting to be collected.
--- @param self -- The InstanceTracker instance.
--- @return boolean -- True if there are changes to collect.
function InstanceTracker.hasPendingChanges(self: InstanceTracker): boolean
	local session = self._session
	return session ~= nil and session.hasDirty
end

--- Returns whether the included instances moved, requiring everything to be tracked again with `watchAll`.
--- @param self -- The InstanceTracker instance.
--- @return boolean -- True if tracking must be restarted.
function InstanceTracker.needsRebuild(self: InstanceTracker): boolean
	local session = self._session
	return session ~= nil and session.needsRebuild
end

--- Reconciles the instances which changed since changes were last collected, updating the mirror.
--- Yields when the frame budget is used up.
--- @param self -- The InstanceTracker instance.
--- @return { types.Change }? -- The changes to apply to the previously encoded tree, or nil if not tracking.
function InstanceTracker.collectChanges(self: InstanceTracker): { types.Change }?
	local session = self._session
	if not session then
		return nil
	elseif session.needsRebuild then
		return {}
	end

	for _, connection in session.droppedConnections do
		connection:Disconnect()
	end
	table.clear(session.droppedConnections)

	local dirty = session.dirty
	session.dirty = {}
	session.hasDirty = false

	local changes: { types.Change } = {}
	local budget = FrameBudget.new(FRAME_BUDGET_SECONDS)

	-- Find tracked instances which are no longer under their tracked parent, and renamed instances
	local detached: { [Instance]: true } = {}
	for instance in dirty do
		if budget:step() and not session.active then
			return nil
		end

		local node = session.nodes[instance]
		if not node then
			continue
		end

		if instance.Parent ~= node.parent then
			if not node.fullyIncluded or session.includeSet[instance] then
				session.needsRebuild = true
				return changes
			end
			detached[instance] = true
		end

		local name = instance.Name
		if name ~= node.name then
			node.name = name
			warnIfInvalidName(instance, name)
			-- Moved instances are sent with their new name
			if not detached[instance] then
				table.insert(changes, { Type = "Update", Id = node.id, Name = encodeName(name) } :: types.Change)
			end
		end
	end

	-- Track moved and added instances under their new parent
	local visited: { [Instance]: true } = {}
	for instance in dirty do
		if budget:step() and not session.active then
			return nil
		end

		self:_attach(session, instance, detached, visited, changes, budget)
		if not session.active then
			return nil
		end
	end

	-- Instances still detached are no longer under a tracked parent
	for instance in detached do
		local node = session.nodes[instance]
		if not node or not detached[instance] then
			continue
		end

		-- Instances with a removed ancestor are removed along with it
		local ancestorRemoved = false
		local ancestor = node.parent
		while ancestor do
			if detached[ancestor] then
				ancestorRemoved = true
				break
			end
			local ancestorNode = session.nodes[ancestor]
			ancestor = ancestorNode and ancestorNode.parent
		end

		if not ancestorRemoved then
			table.insert(changes, { Type = "Remove", Id = node.id } :: types.Change)
			dropNode(session, instance)
		end
	end

	-- Instances can move while this yields such that a dirty instance could not be attached, without any event
	-- being fired for that instance afterwards. Reconcile again those which should now be tracked
	for instance in visited do
		local parent = instance.Parent
		local parentNode = parent and session.nodes[parent]
		if not session.nodes[instance] and parentNode and includeChild(session, instance, parentNode) then
			self:_markDirty(session, instance)
		end
	end

	return changes
end

--- Updates the file paths of tracked instances.
--- @param self -- The InstanceTracker instance.
--- @param instanceFilePaths -- A mapping of instances to their Luau file paths.
--- @return { types.Change } -- The changes to apply to the previously encoded tree.
function InstanceTracker.updateFilePaths(
	self: InstanceTracker,
	instanceFilePaths: { [Instance]: { string } }
): { types.Change }
	local session = self._session
	local changes: { types.Change } = {}
	if not session then
		return changes
	end

	for instance in session.withFilePaths do
		if not instanceFilePaths[instance] then
			local node = session.nodes[instance]
			node.filePaths = nil
			session.withFilePaths[instance] = nil
			table.insert(changes, { Type = "Update", Id = node.id, FilePaths = {} } :: types.Change)
		end
	end

	for instance, filePaths in instanceFilePaths do
		local node = session.nodes[instance]
		if not node then
			continue
		end

		local oldFilePaths = node.filePaths
		local changed = not oldFilePaths or #oldFilePaths ~= #filePaths
		if oldFilePaths and not changed then
			for i, filePath in filePaths do
				if oldFilePaths[i] ~= filePath then
					changed = true
					break
				end
			end
		end

		if changed then
			node.filePaths = filePaths
			session.withFilePaths[instance] = true
			table.insert(changes, { Type = "Update", Id = node.id, FilePaths = filePaths } :: types.Change)
		end
	end

	session.filePaths = instanceFilePaths
	return changes
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

--- Encodes the tracked tree into a serializable table, without traversing the DataModel.
--- Yields when the frame budget is used up.
--- @param self -- The InstanceTracker instance.
--- @return types.EncodedInstance? -- The encoded representation of the DataModel, or nil if not tracking.
function InstanceTracker.encodeAll(self: InstanceTracker): types.EncodedInstance?
	local session = self._session
	if not session then
		return nil
	end

	local encoded = self:_encode(session, self._root, FrameBudget.new(FRAME_BUDGET_SECONDS))
	return if session.active then encoded else nil
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
