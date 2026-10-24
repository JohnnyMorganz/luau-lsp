--- Tests the connection to the language server: sending the full DataModel and changes, handling failures, and
--- connecting and disconnecting while requests are in progress. The language server is replaced by a fake one,
--- which applies the DataModel it receives to a ReferenceServer.

local serde = require("@lune/serde")
local task = require("@lune/task")

local MockDataModel = require("./MockDataModel")
local ReferenceServer = require("./ReferenceServer")

local function assertEqual(actual, expected, message: string?)
	if actual ~= expected then
		error(`{message or "values differ"}\n  expected: {expected}\n  actual:   {actual}`, 2)
	end
end

local function newWorld()
	local game = MockDataModel.new("DataModel", "Game")
	local workspace = MockDataModel.new("Workspace", "Workspace", game)
	local replicatedStorage = MockDataModel.new("ReplicatedStorage", "ReplicatedStorage", game)
	local lighting = MockDataModel.new("Lighting", "Lighting", game)
	local nestedInclude = MockDataModel.new("Folder", "NestedInclude", lighting)
	for i = 1, 3 do
		MockDataModel.new("Part", "Part", MockDataModel.new("Model", `Model{i}`, workspace))
	end
	local module = MockDataModel.new("ModuleScript", "Module", replicatedStorage)
	MockDataModel.flush()
	return {
		game = game,
		workspace = workspace,
		lighting = lighting,
		nestedInclude = nestedInclude,
		module = module,
		includeList = { workspace, replicatedStorage, nestedInclude },
	}
end

local function response(statusCode: number, body: string)
	return { Success = statusCode < 300, StatusCode = statusCode, StatusMessage = "", Headers = {}, Body = body }
end

--- A fake language server, reached through HttpService:RequestAsync.
local function newServer(world)
	local server = {
		-- Endpoints requested, in order
		requests = {},
		-- Seconds to wait before responding, queued per endpoint
		delays = {},
		-- Replaces the response of an endpoint: returns a response, or errors like RequestAsync
		overrides = {},
		files = {},
		syncedInstances = {},
		model = nil,
		session = nil,
	}

	function server.handle(request)
		local endpoint = assert(string.match(request.Url, "/([%w%-]+)$"))
		table.insert(server.requests, endpoint)
		local delay = server.delays[endpoint] and table.remove(server.delays[endpoint], 1)
		if delay then
			task.wait(delay)
		end
		if server.overrides[endpoint] then
			return server.overrides[endpoint](request)
		end

		if endpoint == "get-file-paths" then
			return response(200, serde.encode("json", { files = server.files }))
		elseif endpoint == "full" then
			local tree = serde.decode("json", request.Body).tree
			server.model = ReferenceServer.new(tree)
			server.session = tree.Session
			return response(200, "OK")
		elseif endpoint == "changes" then
			local body = serde.decode("json", request.Body)
			if not server.model or body.session ~= server.session then
				return response(409, "out of sync")
			end
			server.model:apply(body.changes)
			return response(200, "OK")
		end
		error(`unknown endpoint {endpoint}`)
	end

	function server.count(endpoint: string): number
		local count = 0
		for _, requested in server.requests do
			if requested == endpoint then
				count += 1
			end
		end
		return count
	end

	function server.assertSynced()
		MockDataModel.flush()
		assert(server.model, "no DataModel was sent")
		local filePaths = {}
		for filePath, instance in server.syncedInstances do
			filePaths[instance] = { filePath }
		end
		local expected = ReferenceServer.serializeExpected(world.game, world.includeList, filePaths)
		assertEqual(server.model:serialize(), expected, "server DataModel differs from a full encode")
	end

	return server
end

--- Loads the plugin with mock Roblox services which send requests to the fake server.
local function setup(context)
	local world = newWorld()
	local server = newServer(world)

	local HttpService = {}
	function HttpService.JSONEncode(_, value)
		return serde.encode("json", value)
	end
	function HttpService.JSONDecode(_, value)
		return serde.decode("json", value)
	end
	local sessions = 0
	function HttpService.GenerateGUID(_)
		sessions += 1
		return `session-{sessions}`
	end
	function HttpService.RequestAsync(_, request)
		return server.handle(request)
	end

	local services = {
		HttpService = HttpService,
		InstanceFileSyncService = {
			StatusChanged = {
				Connect = function()
					return { Disconnect = function() end }
				end,
			},
			GetAllInstances = function()
				return {}
			end,
			GetSyncedInstance = function(_, filePath)
				return server.syncedInstances[filePath]
			end,
		},
		ScriptEditorService = {},
		TestService = {},
	}

	local require = context.loadPlugin({
		game = {
			GetService = function(_, name)
				return services[name]
			end,
		},
		Enum = { HttpCompression = { Gzip = "Gzip" } },
	})

	local tracker = require("InstanceTracker").new(function()
		return world.includeList
	end, world.game)
	local manager = require("LSPManager").new({
		get = function(_, key)
			return ({ host = "localhost", port = 3667 })[key]
		end,
	}, tracker)
	manager:init()

	return world, server, manager, tracker
end

local tests = {}

local function test(name: string, run: (any) -> ())
	table.insert(tests, {
		name = name,
		run = function(context)
			MockDataModel.setDeferred(false)
			run(context)
		end,
	})
end

test("connecting sends the full DataModel, then changes", function(context)
	local world, server, manager = setup(context)
	assert(manager:connect(), "failed to connect")
	server.assertSynced()

	world.workspace.Model1.Name = "Renamed"
	MockDataModel.new("Part", "Added", world.workspace)
	manager:sync()
	server.assertSynced()
	assertEqual(server.count("full"), 1)
	assertEqual(server.count("changes"), 1)
	manager:unload()
end)

test("the full DataModel is resent when the server does not have it", function(context)
	local world, server, manager = setup(context)
	assert(manager:connect(), "failed to connect")

	-- e.g. the language server restarted
	server.model = nil
	world.workspace.Model1.Name = "Renamed"
	manager:sync()
	server.assertSynced()
	assertEqual(server.count("full"), 2)
	manager:unload()
end)

test("the full DataModel is resent when sending changes fails", function(context)
	local world, server, manager = setup(context)
	assert(manager:connect(), "failed to connect")

	server.overrides.changes = function()
		error("HttpError: Timedout", 0)
	end
	world.workspace.Model1.Name = "Renamed"
	manager:sync()
	server.assertSynced()
	assert(manager:isConnected(), "disconnected after a timeout")
	assertEqual(server.count("full"), 2)
	manager:unload()
end)

test("full DataModels are sent to servers which do not support changes", function(context)
	local world, server, manager = setup(context)
	server.overrides.changes = function()
		return response(404, "Not Found")
	end
	assert(manager:connect(), "failed to connect")

	local model = world.workspace.Model1
	for i = 1, 3 do
		model.Name = `Renamed{i}`
		manager:sync()
		server.assertSynced()
	end
	assertEqual(server.count("changes"), 1)
	assertEqual(server.count("full"), 4)
	manager:unload()
end)

test("disconnects when the server is unreachable", function(context)
	local world, server, manager = setup(context)
	assert(manager:connect(), "failed to connect")

	server.overrides.changes = function()
		error("HttpError: ConnectFail", 0)
	end
	world.workspace.Model1.Name = "Renamed"
	manager:sync()
	assert(not manager:isConnected(), "still connected")
	manager:unload()
end)

test("connecting fails when the server is unreachable", function(context)
	local _, server, manager, tracker = setup(context)
	server.overrides["get-file-paths"] = function()
		error("HttpError: ConnectFail", 0)
	end
	assert(not manager:connect(), "connected")
	assert(not manager:isConnecting(), "still connecting")
	assertEqual(tracker:encodeAll(), nil, "still tracking instances")
	manager:unload()
end)

test("changes made while connecting are sent once connected", function(context)
	local world, server, manager = setup(context)
	local changed = false
	context.budget.yieldChance = 1
	context.budget.onYield = function()
		if not changed then
			changed = true
			world.workspace.Model2.Name = "RenamedWhileConnecting"
		end
	end
	assert(manager:connect(), "failed to connect")
	context.budget.yieldChance = 0

	manager:sync()
	server.assertSynced()
	manager:unload()
end)

test("disconnecting while connecting stops connecting", function(context)
	local _, server, manager, tracker = setup(context)
	server.delays["get-file-paths"] = { 0.1 }
	local connected = nil
	task.spawn(function()
		connected = manager:connect()
	end)
	assert(manager:isConnecting(), "not connecting")
	manager:disconnect()
	task.wait(0.2)

	assertEqual(connected, false)
	assert(not manager:isConnected(), "connected")
	assertEqual(server.count("full"), 0)
	assertEqual(tracker:encodeAll(), nil, "still tracking instances")
	manager:unload()
end)

test("reconnecting while a sync is in progress stays connected", function(context)
	-- Settings changes reconnect, which can happen while the previous connection is syncing
	local world, server, manager = setup(context)
	server.files = { "Module.luau" }
	server.syncedInstances["Module.luau"] = world.module
	assert(manager:connect(), "failed to connect")

	server.delays["get-file-paths"] = { 0.2, 0.3 }
	world.workspace.Model1.Name = "Renamed"
	task.spawn(manager.sync, manager)
	task.wait(0.05)
	manager:disconnect()
	task.spawn(manager.connect, manager)
	task.wait(0.6)

	assert(manager:isConnected(), "disconnected after reconnecting")
	server.assertSynced()
	manager:unload()
end)

test("reconnecting while resending the full DataModel stays connected", function(context)
	local world, server, manager, tracker = setup(context)
	assert(manager:connect(), "failed to connect")

	-- Moving an included instance requires tracking and sending everything again
	world.nestedInclude.Parent = world.workspace
	assert(tracker:needsRebuild(), "expected a rebuild")
	server.delays["get-file-paths"] = { 0.3, 0.05 }
	task.spawn(manager.sync, manager)
	task.wait(0.05)
	manager:disconnect()
	task.spawn(manager.connect, manager)
	task.wait(0.6)

	assert(manager:isConnected(), "disconnected after reconnecting")
	server.assertSynced()
	manager:unload()
end)

test("disconnecting while resending the full DataModel stops tracking", function(context)
	local world, server, manager, tracker = setup(context)
	assert(manager:connect(), "failed to connect")

	world.nestedInclude.Parent = world.workspace
	server.delays["get-file-paths"] = { 0.2 }
	task.spawn(manager.sync, manager)
	task.wait(0.05)
	manager:disconnect()
	task.wait(0.3)

	assertEqual(tracker:encodeAll(), nil, "still tracking instances")
	assertEqual(server.count("full"), 1)
	manager:unload()
end)

return tests
