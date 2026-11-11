--- A minimal in-memory model of Roblox Instances, used to test the plugin outside of Roblox Studio.
---
--- It models the behaviour the plugin relies on:
--- - `Name` and `Parent` property changed signals, `AncestryChanged`, `DescendantAdded` and `DescendantRemoving`
--- - `DescendantAdded`/`DescendantRemoving` only fire on ancestors that are gained or lost, so moving an instance
---   between two children of the same ancestor does not fire them on that ancestor (as in Roblox)
--- - `Destroy` parents the instance and all its descendants to nil, and disconnects their connections
--- - Deferred events: handlers run when `flush` is called (Roblox runs them at resumption points). Immediate events
---   can be enabled with `setDeferred(false)`. Whether handlers of connections disconnected before `flush` (including
---   by `Destroy`) are still called is configurable with `setDropDisconnectedEvents`, so both behaviours are tested

local MockDataModel = {}

local deferred = true
local dropDisconnectedEvents = false
local queue: { { connection: any, args: { any } } } = {}

--- Sets whether event handlers are deferred until `flush` is called, or run immediately.
function MockDataModel.setDeferred(value: boolean)
	deferred = value
end

--- Sets whether deferred handlers of connections which were disconnected before `flush` are dropped.
function MockDataModel.setDropDisconnectedEvents(value: boolean)
	dropDisconnectedEvents = value
end

--- Runs the deferred event handlers, including those of events fired while running them.
function MockDataModel.flush()
	local index = 1
	while index <= #queue do
		local entry = queue[index]
		if entry.connection.Connected or not dropDisconnectedEvents then
			entry.connection._handler(table.unpack(entry.args))
		end
		index += 1
	end
	table.clear(queue)
end

local function newSignal()
	local signal = { _connections = {} }

	function signal:Connect(handler)
		local connection = { Connected = true, _handler = handler }
		function connection.Disconnect()
			connection.Connected = false
			local index = table.find(signal._connections, connection)
			if index then
				table.remove(signal._connections, index)
			end
		end
		table.insert(signal._connections, connection)
		return connection
	end

	function signal:_fire(...)
		for _, connection in table.clone(signal._connections) do
			if deferred then
				table.insert(queue, { connection = connection, args = table.pack(...) })
			else
				connection._handler(...)
			end
		end
	end

	function signal:_disconnectAll()
		for _, connection in table.clone(signal._connections) do
			connection:Disconnect()
		end
	end

	return signal
end

local Instance = {}

local function ancestors(instance): { any }
	local result = {}
	local current = instance
	while current do
		table.insert(result, current)
		current = rawget(current, "_parent")
	end
	return result
end

local function subtree(instance, result)
	table.insert(result, instance)
	for _, child in instance._children do
		subtree(child, result)
	end
	return result
end

local methods = {}

function methods:GetChildren()
	return table.clone(self._children)
end

function methods:GetDescendants()
	local result = subtree(self, {})
	table.remove(result, 1)
	return result
end

function methods:IsAncestorOf(other)
	local current = other._parent
	while current do
		if current == self then
			return true
		end
		current = current._parent
	end
	return false
end

function methods:IsDescendantOf(other)
	return other:IsAncestorOf(self)
end

function methods:FindFirstChild(name)
	for _, child in self._children do
		if child._name == name then
			return child
		end
	end
	return nil
end

function methods:GetFullName()
	local names = {}
	for _, ancestor in ancestors(self) do
		if ancestor._className ~= "DataModel" then
			table.insert(names, 1, ancestor._name)
		end
	end
	return table.concat(names, ".")
end

function methods:GetPropertyChangedSignal(property)
	local signal = self._propertySignals[property]
	if not signal then
		signal = newSignal()
		self._propertySignals[property] = signal
	end
	return signal
end

function methods:Destroy()
	self.Parent = nil
	self._parentLocked = true
	for _, signal in self._propertySignals do
		signal:_disconnectAll()
	end
	self.AncestryChanged:_disconnectAll()
	self.DescendantAdded:_disconnectAll()
	self.DescendantRemoving:_disconnectAll()
	for _, child in table.clone(self._children) do
		child:Destroy()
	end
end

local function setParent(instance, newParent)
	if instance._parentLocked then
		error(`The Parent property of {instance._name} is locked`)
	end
	local oldParent = instance._parent
	if newParent == oldParent then
		return
	end
	if newParent == instance or (newParent and instance:IsAncestorOf(newParent)) then
		error(`Attempt to set parent of {instance._name} to a descendant`)
	end

	local oldAncestors = oldParent and ancestors(oldParent) or {}
	local newAncestors = newParent and ancestors(newParent) or {}
	local moved = subtree(instance, {})

	for _, ancestor in oldAncestors do
		if not table.find(newAncestors, ancestor) then
			for _, descendant in moved do
				ancestor.DescendantRemoving:_fire(descendant)
			end
		end
	end

	if oldParent then
		table.remove(oldParent._children, table.find(oldParent._children, instance))
	end
	rawset(instance, "_parent", newParent)
	if newParent then
		table.insert(newParent._children, instance)
	end

	local parentSignal = instance._propertySignals.Parent
	if parentSignal then
		parentSignal:_fire()
	end
	for _, descendant in moved do
		descendant.AncestryChanged:_fire(instance, newParent)
	end

	for _, ancestor in newAncestors do
		if not table.find(oldAncestors, ancestor) then
			for _, descendant in moved do
				ancestor.DescendantAdded:_fire(descendant)
			end
		end
	end
end

Instance.__index = function(self, key)
	if key == "Name" then
		return self._name
	elseif key == "ClassName" then
		return self._className
	elseif key == "Parent" then
		return self._parent
	end
	return methods[key] or self:FindFirstChild(key)
end

Instance.__newindex = function(self, key, value)
	if key == "Name" then
		if self._name ~= value then
			self._name = value
			local signal = self._propertySignals.Name
			if signal then
				signal:_fire()
			end
		end
	elseif key == "Parent" then
		setParent(self, value)
	else
		error(`{key} is not a valid member of {self._className}`)
	end
end

Instance.__tostring = function(self)
	return self._name
end

--- Creates a new mock instance.
function MockDataModel.new(className: string, name: string?, parent: any?): any
	local instance = setmetatable({
		_name = name or className,
		_className = className,
		_parent = nil,
		_children = {},
		_propertySignals = {},
		_parentLocked = false,
	}, Instance)
	rawset(instance, "AncestryChanged", newSignal())
	rawset(instance, "DescendantAdded", newSignal())
	rawset(instance, "DescendantRemoving", newSignal())
	if parent then
		instance.Parent = parent
	end
	return instance
end

return MockDataModel
