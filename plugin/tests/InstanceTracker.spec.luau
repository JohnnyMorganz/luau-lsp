--- Tests that the changes produced by the InstanceTracker keep the language server's DataModel identical to
--- encoding the DataModel from scratch, which is what the plugin sent on every change before.

local MockDataModel = require("./MockDataModel")
local ReferenceServer = require("./ReferenceServer")

local INVALID_UTF8 = "Bad\255Name"

local function assertEqual(actual, expected, message: string?)
	if actual ~= expected then
		error(`{message or "values differ"}\n  expected: {expected}\n  actual:   {actual}`, 2)
	end
end

--- Creates a DataModel with included services, a non-included service, and an included instance nested in a
--- non-included service.
local function newWorld()
	local game = MockDataModel.new("DataModel", "Game")
	local workspace = MockDataModel.new("Workspace", "Workspace", game)
	local replicatedStorage = MockDataModel.new("ReplicatedStorage", "ReplicatedStorage", game)
	local coreGui = MockDataModel.new("CoreGui", "CoreGui", game)
	local lighting = MockDataModel.new("Lighting", "Lighting", game)
	local sky = MockDataModel.new("Folder", "Sky", lighting)
	local nestedInclude = MockDataModel.new("Folder", "NestedInclude", sky)
	MockDataModel.new("Folder", "NotIncluded", lighting)

	for i = 1, 3 do
		local model = MockDataModel.new("Model", `Model{i}`, workspace)
		for j = 1, 3 do
			MockDataModel.new("Part", `Part{j}`, model)
		end
	end
	MockDataModel.new("ModuleScript", "Module", replicatedStorage)
	MockDataModel.new("Folder", "Gui", coreGui)
	MockDataModel.flush()

	return {
		game = game,
		workspace = workspace,
		replicatedStorage = replicatedStorage,
		coreGui = coreGui,
		lighting = lighting,
		sky = sky,
		nestedInclude = nestedInclude,
		includeList = { workspace, replicatedStorage, nestedInclude },
		filePaths = {},
	}
end

local function track(context, world)
	local InstanceTracker = context.require("InstanceTracker")
	local tracker = InstanceTracker.new(function()
		return world.includeList
	end, world.game)
	assert(tracker:watchAll(world.filePaths), "watchAll was stopped")
	return tracker, ReferenceServer.new(tracker:encodeAll())
end

--- Sends the pending changes to the server, as the LSPManager does.
local function syncOnce(tracker, server)
	MockDataModel.flush()
	local changes = assert(tracker:collectChanges(), "collectChanges returned nil")
	server:apply(changes)
	return changes
end

--- Sends the pending changes to the server until there are none left, returning all the changes sent.
local function sync(tracker, server)
	local changes = {}
	for _ = 1, 10 do
		local batch = syncOnce(tracker, server)
		table.move(batch, 1, #batch, #changes + 1, changes)
		MockDataModel.flush()
		if tracker:needsRebuild() or not tracker:hasPendingChanges() then
			break
		end
	end
	return changes
end

local function assertSynced(world, tracker, server)
	MockDataModel.flush()
	assert(not tracker:hasPendingChanges(), "tracker has unsynced changes")
	local expected = ReferenceServer.serializeExpected(world.game, world.includeList, world.filePaths)
	assertEqual(server:serialize(), expected, "server DataModel differs from a full encode")
	assertEqual(
		ReferenceServer.serializeEncoded(tracker:encodeAll()),
		expected,
		"tracked DataModel differs from a full encode"
	)
end

local tests = {}

local function test(name: string, run: (any) -> ())
	table.insert(tests, { name = name, run = run })
end

test("tracks the included instances", function(context)
	local world = newWorld()
	local tracker, server = track(context, world)
	assertSynced(world, tracker, server)

	local encoded = server:serialize()
	assert(string.find(encoded, "NestedInclude", 1, true), "nested included instance is missing")
	assert(not string.find(encoded, "CoreGui", 1, true), "non-included service was sent")
	assert(not string.find(encoded, "NotIncluded", 1, true), "non-included sibling was sent")
end)

test("no changes are sent if nothing changed", function(context)
	local world = newWorld()
	local tracker, server = track(context, world)
	assertEqual(#sync(tracker, server), 0)
end)

test("renaming an instance sends a single update", function(context)
	local world = newWorld()
	local tracker, server = track(context, world)

	world.workspace.Model1.Part1.Name = "Renamed"
	local changes = sync(tracker, server)
	assertEqual(#changes, 1)
	assertEqual(changes[1].Type, "Update")
	assertEqual(changes[1].Name, "Renamed")
	assertSynced(world, tracker, server)
end)

test("rapid consecutive changes are batched", function(context)
	local world = newWorld()
	local tracker, server = track(context, world)

	local part = world.workspace.Model1.Part1
	for i = 1, 10 do
		part.Name = `Name{i}`
		part.Parent = if i % 2 == 0 then world.workspace.Model2 else world.workspace.Model3
	end
	local changes = sync(tracker, server)
	assertEqual(#changes, 1)
	assertEqual(changes[1].Name, "Name10")
	assertSynced(world, tracker, server)
end)

test("reparenting a large subtree sends a single update", function(context)
	local world = newWorld()
	local model = world.workspace.Model1
	for i = 1, 200 do
		MockDataModel.new("Part", `Extra{i}`, model)
	end
	local tracker, server = track(context, world)

	-- Fires DescendantRemoving and DescendantAdded for every descendant
	model.Parent = world.replicatedStorage
	local changes = sync(tracker, server)
	assertEqual(#changes, 1)
	assertEqual(changes[1].Type, "Update")
	assertSynced(world, tracker, server)
end)

test("adding a subtree sends a single addition", function(context)
	local world = newWorld()
	local tracker, server = track(context, world)

	local model = MockDataModel.new("Model", "NewModel")
	for i = 1, 50 do
		MockDataModel.new("Part", `Part{i}`, MockDataModel.new("Folder", `Folder{i}`, model))
	end
	model.Parent = world.workspace
	local changes = sync(tracker, server)
	assertEqual(#changes, 1)
	assertEqual(changes[1].Type, "Add")
	assertSynced(world, tracker, server)
end)

test("destroying a subtree sends a single removal", function(context)
	local world = newWorld()
	local tracker, server = track(context, world)

	world.workspace.Model1:Destroy()
	local changes = sync(tracker, server)
	assertEqual(#changes, 1)
	assertEqual(changes[1].Type, "Remove")
	assertSynced(world, tracker, server)
end)

test("instances parented to nil and back are removed and added again", function(context)
	local world = newWorld()
	local tracker, server = track(context, world)

	local model = world.workspace.Model1
	model.Parent = nil
	assertEqual(sync(tracker, server)[1].Type, "Remove")
	assertSynced(world, tracker, server)

	model.Parent = world.workspace
	assertEqual(sync(tracker, server)[1].Type, "Add")
	assertSynced(world, tracker, server)
end)

test("instances moved between included and non-included instances", function(context)
	local world = newWorld()
	local tracker, server = track(context, world)

	world.workspace.Model1.Parent = world.coreGui
	assertEqual(sync(tracker, server)[1].Type, "Remove")
	assertSynced(world, tracker, server)

	-- No DescendantAdded is fired on the DataModel for this move, as it was already an ancestor
	world.coreGui.Gui.Parent = world.workspace
	assertEqual(sync(tracker, server)[1].Type, "Add")
	assertSynced(world, tracker, server)

	world.coreGui.Model1.Parent = world.nestedInclude
	sync(tracker, server)
	assertSynced(world, tracker, server)
end)

test("invalid UTF-8 in names is replaced", function(context)
	local world = newWorld()
	local bad = MockDataModel.new("Folder", INVALID_UTF8, world.workspace)
	MockDataModel.new("Part", "Child", bad)
	local tracker, server = track(context, world)
	assertSynced(world, tracker, server)
	local replaced = `Bad{utf8.char(0xFFFD)}Name`
	assert(string.find(server:serialize(), replaced .. "<Folder>(){Child<Part>", 1, true), "name was not replaced")

	bad.Name = "Good"
	assertEqual(sync(tracker, server)[1].Name, "Good")
	assertSynced(world, tracker, server)

	bad.Name = INVALID_UTF8
	assertEqual(sync(tracker, server)[1].Name, replaced)
	assertSynced(world, tracker, server)
end)

test("file paths are updated", function(context)
	local world = newWorld()
	local module = world.replicatedStorage.Module
	world.filePaths = { [module] = { "src/Module.luau" } }
	local tracker, server = track(context, world)
	assertSynced(world, tracker, server)

	world.filePaths = { [module] = { "src/Renamed.luau" } }
	server:apply(tracker:updateFilePaths(world.filePaths))
	assertSynced(world, tracker, server)

	world.filePaths = {}
	server:apply(tracker:updateFilePaths(world.filePaths))
	assertSynced(world, tracker, server)
end)

test("moving an included instance requires tracking everything again", function(context)
	local world = newWorld()
	local tracker, server = track(context, world)

	world.nestedInclude.Parent = world.coreGui
	sync(tracker, server)
	assert(tracker:needsRebuild(), "expected a rebuild")
end)

test("tracking can be stopped while in progress", function(context)
	local world = newWorld()
	local InstanceTracker = context.require("InstanceTracker")
	local tracker = InstanceTracker.new(function()
		return world.includeList
	end, world.game)

	context.budget.yieldChance = 1
	context.budget.onYield = function()
		tracker:unwatchAll()
	end
	assertEqual(tracker:watchAll({}), false)
	assertEqual(tracker:collectChanges(), nil)
	assertEqual(tracker:encodeAll(), nil)
end)

test("children are encoded in the same order as GetChildren", function(context)
	local world = newWorld()
	local folder = MockDataModel.new("Folder", "Folder", world.workspace)
	for _, name in { "A", "B", "C" } do
		MockDataModel.new("Part", name, folder)
	end
	local tracker, server = track(context, world)

	-- Moved instances are added to the end of their new parent's children
	folder.A.Parent = world.workspace
	sync(tracker, server)
	world.workspace.A.Parent = folder
	sync(tracker, server)

	local function names(instance)
		local result = {}
		for _, child in instance.Children or instance:GetChildren() do
			table.insert(result, child.Name)
		end
		return table.concat(result, ",")
	end
	local encodedFolder
	for _, child in tracker:encodeAll().Children do
		if child.Name == "Workspace" then
			for _, workspaceChild in child.Children do
				if workspaceChild.Name == "Folder" then
					encodedFolder = workspaceChild
				end
			end
		end
	end
	assertEqual(names(encodedFolder), names(folder))
	assertEqual(names(folder), "B,C,A")
end)

test("instances moved under a moved instance while tracking yields are not lost", function(context)
	-- Workspace.A.B.P: B moves to Workspace, then while the tracker yields, P moves to Workspace and B moves under P.
	-- The events for these moves are only handled after the tracker has stopped tracking B and P
	MockDataModel.setDropDisconnectedEvents(true)
	local world = newWorld()
	local b = MockDataModel.new("Folder", "B", MockDataModel.new("Folder", "A", world.workspace))
	local p = MockDataModel.new("Folder", "P", b)
	local tracker, server = track(context, world)

	b.Parent = world.workspace
	MockDataModel.flush()
	local steps = 0
	context.budget.yieldChance = 1
	context.budget.onYield = function()
		steps += 1
		if steps == 2 then
			p.Parent = world.workspace
			b.Parent = p
		end
	end
	syncOnce(tracker, server)
	context.budget.yieldChance = 0
	sync(tracker, server)
	assertSynced(world, tracker, server)
end)

--- Makes a random change to the DataModel.
local function mutate(world)
	local instances = world.game:GetDescendants()
	for _, instance in world.detached do
		if instance.Parent == nil and not instance._parentLocked then
			table.insert(instances, instance)
		end
	end

	if #instances == 0 then
		return
	end
	local function randomInstance()
		return instances[math.random(#instances)]
	end
	local function randomName()
		local names = { "A", "B", "Part", "Model", INVALID_UTF8 }
		return names[math.random(if math.random() < 0.05 then 5 else 4)]
	end
	local function isProtected(instance)
		-- Services and the ancestors of included instances rarely change
		return instance.Parent == world.game or instance == world.sky or instance == world.nestedInclude
	end

	local roll = math.random(100)
	if roll <= 30 then
		local instance = MockDataModel.new(if math.random(2) == 1 then "Model" else "Part", randomName())
		for _ = 1, math.random(0, 3) do
			MockDataModel.new("Part", randomName(), instance)
		end
		instance.Parent = randomInstance()
	elseif roll <= 50 then
		local instance = randomInstance()
		if not isProtected(instance) or math.random() < 0.05 then
			instance.Name = randomName()
		end
	elseif roll <= 80 then
		local instance, newParent = randomInstance(), randomInstance()
		if not isProtected(instance) or math.random() < 0.05 then
			if math.random() < 0.15 then
				newParent = nil
				table.insert(world.detached, instance)
			end
			if newParent ~= instance and not (newParent and instance:IsAncestorOf(newParent)) then
				instance.Parent = newParent
			end
		end
	else
		local instance = randomInstance()
		if not isProtected(instance) then
			instance:Destroy()
		end
	end
end

--- Syncs until there are no more changes, tracking everything again if needed as the LSPManager does.
local function syncUntilSettled(context, world, tracker, server)
	for _ = 1, 20 do
		syncOnce(tracker, server)
		if tracker:needsRebuild() then
			assert(tracker:watchAll(world.filePaths), "watchAll was stopped")
			server = ReferenceServer.new(tracker:encodeAll())
		end
		MockDataModel.flush()
		if not tracker:hasPendingChanges() then
			return server
		end
		-- Stop modifying the DataModel while settling, so that it eventually settles
		context.budget.yieldChance = 0
	end
	error("changes did not settle")
end

local function fuzz(context, seed: number, deferred: boolean)
	math.randomseed(seed)
	MockDataModel.setDeferred(deferred)
	MockDataModel.setDropDisconnectedEvents(deferred and seed % 2 == 0)

	local world = newWorld()
	world.detached = {}
	-- Modify the DataModel whenever the tracker yields, including while it tracks the initial DataModel
	context.budget.onYield = function()
		for _ = 1, math.random(3) do
			mutate(world)
		end
		if math.random() < 0.5 then
			MockDataModel.flush()
		end
	end
	context.budget.yieldChance = 0.05

	local tracker, server = track(context, world)
	server = syncUntilSettled(context, world, tracker, server)
	assertSynced(world, tracker, server)

	for _ = 1, 150 do
		for _ = 1, math.random(10) do
			mutate(world)
			if math.random() < 0.3 then
				MockDataModel.flush()
			end
		end

		context.budget.yieldChance = 0.05
		local success, err = pcall(function()
			server = syncUntilSettled(context, world, tracker, server)
			assertSynced(world, tracker, server)
		end)
		if not success then
			error(`seed {seed}: {err}`)
		end
	end
end

for seed = 1, 25 do
	test(`random changes stay in sync with deferred events (seed {seed})`, function(context)
		fuzz(context, seed, true)
	end)
	test(`random changes stay in sync with immediate events (seed {seed})`, function(context)
		fuzz(context, seed, false)
	end)
end

return tests
