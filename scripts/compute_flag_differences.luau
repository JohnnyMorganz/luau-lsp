--!strict
--- Given a `luau-lsp` binary, computes the differences between default flags and flags currently enabled for Roblox
--- Useful for debugging flag differences
local net = require("@lune/net")
local process = require("@lune/process")

local response =
	net.request("https://clientsettingscdn.roblox.com/v1/settings/application?applicationName=PCDesktopClient")
local fflags = net.jsonDecode(response.body).applicationSettings

local lsp_binary = assert(process.args[1], "Usage: lune run compute_flag_differences.luau <luau_lsp_binary>")
local output = process.spawn(lsp_binary, { "--show-flags" })
local availableFFlags = {}

local lines = output.stdout:split("\n")
for i, line in lines do
	if i == 1 then
		continue
	end
	line = line:gsub("%s+", "")
	if line == "" then
		continue
	end
	availableFFlags[line:split("=")[1]] = line:split("=")[2]
end

local options = { "FFlag", "DFFlag", "FInt", "DFInt" }

for flag, defaultValue in availableFFlags do
	for otherFlag, currentValue in fflags do
		local found = false
		for _, opt in options do
			if opt .. flag == otherFlag then
				found = true

				if defaultValue:lower() ~= currentValue:lower() then
					print(`{flag}: {currentValue} [default: {defaultValue}]`)
				end

				break
			end
		end
		if found then
			break
		end
	end
end
