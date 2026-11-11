--- A model of how the language server applies the DataModel sent by the plugin, used to verify that the changes
--- produced by the plugin result in the same DataModel as encoding it from scratch.

local ReferenceServer = {}
ReferenceServer.__index = ReferenceServer

local function build(server, encoded, parent)
	assert(encoded.Id, `instance {encoded.Name} is missing an Id`)
	assert(not server.nodes[encoded.Id], `duplicate instance Id {encoded.Id}`)

	local node = {
		id = encoded.Id,
		name = encoded.Name,
		className = encoded.ClassName,
		filePaths = table.clone(encoded.FilePaths or {}),
		children = {},
		parent = parent,
	}
	server.nodes[node.id] = node
	for _, child in encoded.Children or {} do
		table.insert(node.children, build(server, child, node))
	end
	return node
end

local function detach(node)
	local siblings = node.parent.children
	table.remove(siblings, assert(table.find(siblings, node)))
	node.parent = nil
end

local function unindex(server, node)
	server.nodes[node.id] = nil
	for _, child in node.children do
		unindex(server, child)
	end
end

--- Creates a server holding a full DataModel.
function ReferenceServer.new(tree)
	local self = setmetatable({ nodes = {} }, ReferenceServer)
	self.root = build(self, tree, nil)
	return self
end

--- Applies incremental changes, erroring if a change is invalid.
function ReferenceServer.apply(self, changes)
	for _, change in changes do
		if change.Type == "Add" then
			local parent = assert(self.nodes[change.Parent], `unknown parent Id {change.Parent}`)
			table.insert(parent.children, build(self, change.Instance, parent))
		elseif change.Type == "Remove" then
			local node = assert(self.nodes[change.Id], `unknown instance Id {change.Id}`)
			assert(node.parent, "cannot remove the root")
			detach(node)
			unindex(self, node)
		elseif change.Type == "Update" then
			local node = assert(self.nodes[change.Id], `unknown instance Id {change.Id}`)
			if change.Name then
				node.name = change.Name
			end
			if change.FilePaths then
				node.filePaths = table.clone(change.FilePaths)
			end
			if change.Parent then
				local parent = assert(self.nodes[change.Parent], `unknown parent Id {change.Parent}`)
				local ancestor = parent
				while ancestor do
					assert(ancestor ~= node, "cannot parent an instance to its own descendant")
					ancestor = ancestor.parent
				end
				detach(node)
				node.parent = parent
				table.insert(parent.children, node)
			end
		else
			error(`unknown change type {change.Type}`)
		end
	end
end

--- Serializes a tree in a canonical form, ignoring the order of children and Ids.
function ReferenceServer.canonical(name: string, className: string, filePaths: { string }, children: { string }): string
	table.sort(children)
	return `{name}<{className}>({table.concat(filePaths, ";")})\{{table.concat(children, ",")}\}`
end

--- Serializes the server's DataModel in a canonical form.
function ReferenceServer.serialize(self, node): string
	node = node or self.root
	local children = {}
	for _, child in node.children do
		table.insert(children, self:serialize(child))
	end
	return ReferenceServer.canonical(node.name, node.className, node.filePaths, children)
end

--- Serializes an encoded tree in a canonical form.
function ReferenceServer.serializeEncoded(encoded): string
	local children = {}
	for _, child in encoded.Children or {} do
		table.insert(children, ReferenceServer.serializeEncoded(child))
	end
	return ReferenceServer.canonical(encoded.Name, encoded.ClassName, encoded.FilePaths or {}, children)
end

--- Replaces each byte which does not start a valid UTF-8 character with U+FFFD.
function ReferenceServer.sanitizeName(name: string): string
	local result = ""
	local position = 1
	while position <= #name do
		local characterLength = nil
		for length = 1, 4 do
			if utf8.len(string.sub(name, position, position + length - 1)) == 1 then
				characterLength = length
				break
			end
		end
		if characterLength then
			result ..= string.sub(name, position, position + characterLength - 1)
			position += characterLength
		else
			result ..= utf8.char(0xFFFD)
			position += 1
		end
	end
	return result
end

--- Serializes the DataModel the plugin should send in a canonical form, by encoding the live instances from
--- scratch with the same rules as the original plugin: an instance is included if it is in, contains, or is one of
--- the included instances. Invalid UTF-8 in names is replaced.
function ReferenceServer.serializeExpected(
	instance,
	includeList: { any },
	instanceFilePaths: { [any]: { string } }
): string
	local children = {}
	for _, child in instance:GetChildren() do
		local included = false
		for _, includedInstance in includeList do
			if
				child == includedInstance
				or child:IsDescendantOf(includedInstance)
				or child:IsAncestorOf(includedInstance)
			then
				included = true
				break
			end
		end

		if included then
			table.insert(children, ReferenceServer.serializeExpected(child, includeList, instanceFilePaths))
		end
	end
	return ReferenceServer.canonical(
		ReferenceServer.sanitizeName(instance.Name),
		instance.ClassName,
		instanceFilePaths[instance] or {},
		children
	)
end

return ReferenceServer
