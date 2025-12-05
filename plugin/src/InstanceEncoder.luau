--!strict

--- Encodes Roblox Instances into a serializable format for the language server.
--- Handles filtering based on include paths and recursive encoding.

local Log = require(script.Parent.Log)
local types = require(script.Parent.types)

local InstanceEncoder = {}

--- Determines whether an Instance should be included in the encoded tree.
--- An instance is included if it is within, contains, or is one of the
--- provided include paths.
--- @param instance -- The Instance to check.
--- @param includeList -- The list of instances to include.
--- @return boolean -- True if the instance should be included.
function InstanceEncoder.shouldInclude(instance: Instance, includeList: { Instance }): boolean
	-- Ignore instances that are not part of any tree
	if instance.Parent == nil then
		return false
	end

	-- Check if this instance is within, contains, or is an included tree
	for _, includedInstance in includeList do
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
--- @param instance -- The Instance to encode.
--- @param includeList -- The list of instances to include.
--- @return types.EncodedInstance -- The encoded representation.
function InstanceEncoder.encode(instance: Instance, includeList: { Instance }): types.EncodedInstance
	Log.debug(`Encoding {instance:GetFullName()}`)

	local encoded: types.EncodedInstance = {
		Name = instance.Name,
		ClassName = instance.ClassName,
		Children = {},
	}

	for _, child in instance:GetChildren() do
		if not InstanceEncoder.shouldInclude(child, includeList) then
			continue
		end

		table.insert(encoded.Children, InstanceEncoder.encode(child, includeList))
	end

	return encoded
end

return InstanceEncoder
