--- Runs the plugin tests outside of Roblox Studio, against a mock DataModel.
--- Usage: `lune run tests/run.luau` from the plugin directory.

local fs = require("@lune/fs")
local luau = require("@lune/luau")
local process = require("@lune/process")
local task = require("@lune/task")

local MockDataModel = require("./MockDataModel")

local SOURCE_DIRECTORY = if fs.isDir("src") then "src" else "plugin/src"

--- A FrameBudget which yields at random, running `onYield` instead of waiting for the next frame.
--- Tests use `onYield` to modify the DataModel while the plugin is in the middle of its work.
local TestFrameBudget = {
	yieldChance = 0,
	onYield = nil :: (() -> ())?,
}
TestFrameBudget.__index = TestFrameBudget

function TestFrameBudget.new(_seconds: number)
	return setmetatable({}, TestFrameBudget)
end

function TestFrameBudget.step(_self): boolean
	if TestFrameBudget.onYield and math.random() < TestFrameBudget.yieldChance then
		TestFrameBudget.onYield()
		return true
	end
	return false
end

--- The plugin source files, read upfront as reading files yields (which is not allowed in metamethods).
local sources: { [string]: string } = {}
local directories: { [string]: boolean } = {}
local function readSources(directory: string)
	directories[directory] = true
	for _, name in fs.readDir(directory) do
		local path = `{directory}/{name}`
		if fs.isDir(path) then
			readSources(path)
		elseif string.match(name, "%.luau$") then
			sources[path] = fs.readFile(path)
		end
	end
end
readSources(SOURCE_DIRECTORY)

--- Represents a module or directory of the plugin source as a Roblox `script`, supporting `.Parent` and children.
local function makeScript(path: string)
	return setmetatable({ _path = path }, {
		__index = function(self, key)
			local parentPath = string.match(self._path, "^(.*)/[^/]*$")
			if key == "Parent" then
				return if parentPath then makeScript(parentPath) else nil
			end
			local directory = if directories[self._path] then self._path else parentPath
			return makeScript(`{directory}/{key}`)
		end,
	})
end

--- Loads the plugin modules, with additional globals (e.g. `game`) available to them.
--- Each call returns a fresh set of modules.
local function loadPlugin(globals: { [string]: any }?)
	local cache = {}

	local function requireModule(module)
		local path = module._path
		if path == `{SOURCE_DIRECTORY}/Utils/FrameBudget` then
			return TestFrameBudget
		end
		if cache[path] then
			return cache[path]
		end

		local filePath = if directories[path] then `{path}/init.luau` else `{path}.luau`
		local environment = { script = module, require = requireModule, task = task, warn = function() end }
		for name, value in globals or {} do
			environment[name] = value
		end
		local chunk = luau.load(assert(sources[filePath], `{filePath} does not exist`), {
			debugName = filePath,
			environment = environment,
		})
		cache[path] = chunk()
		return cache[path]
	end

	local root = makeScript(SOURCE_DIRECTORY)
	return function(name: string)
		return requireModule(root[name])
	end
end

local failures = 0
local count = 0

local function runSpec(path: string)
	local spec = require(path)
	for _, test in spec do
		count += 1
		MockDataModel.setDeferred(true)
		MockDataModel.setDropDisconnectedEvents(false)
		TestFrameBudget.yieldChance = 0
		TestFrameBudget.onYield = nil

		local success, err =
			pcall(test.run, { require = loadPlugin(), loadPlugin = loadPlugin, budget = TestFrameBudget })
		if success then
			print(`[PASS] {test.name}`)
		else
			failures += 1
			print(`[FAIL] {test.name}\n{err}`)
		end
	end
end

runSpec("./InstanceTracker.spec")
runSpec("./LSPManager.spec")

print(`\n{count - failures}/{count} tests passed`)
process.exit(if failures == 0 then 0 else 1)
