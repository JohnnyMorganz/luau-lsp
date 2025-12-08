--!strict

--- HTTP endpoints for communicating with the Luau Language Server.
--- Handles serialization and error handling for LSP requests.

local HttpService = game:GetService("HttpService")

local types = require(script.Parent.types)

local ServerEndpoints = {}

--- Wraps an HTTP request result into a Result type.
--- Handles both pcall failures and HTTP error responses.
--- @param success -- Whether the pcall succeeded.
--- @param result -- The result from pcall (response or error).
--- @return types.Result<types.HttpResponse, string> -- Success with response or failure with error message.
local function wrapHttpResult(success: boolean, result: any): types.Result<types.HttpResponse, string>
	if not success then
		return { ok = false, error = tostring(result) }
	end

	if not result.Success then
		return { ok = false, error = result.StatusCode .. ": " .. result.Body }
	end

	return { ok = true, value = result }
end

--- Sends the full DataModel tree to the language server.
--- The tree is JSON-encoded and compressed with Gzip before sending.
--- @param address -- The full server address (e.g., "http://localhost:3667").
--- @param tree -- The encoded instance tree to send.
--- @return types.Result<types.HttpResponse, string> -- Success with response or failure with error message.
function ServerEndpoints.full(address: string, tree: types.EncodedInstance): types.Result<types.HttpResponse, string>
	local success, result = pcall(HttpService.RequestAsync, HttpService, {
		Method = "POST" :: "POST",
		Url = `{address}/full`,
		Headers = {
			["Content-Type"] = "application/json",
		},
		Body = HttpService:JSONEncode({
			tree = tree,
		}),
		Compress = Enum.HttpCompression.Gzip,
	})

	return wrapHttpResult(success, result)
end

--- Clears the DataModel information from the language server.
--- @param address -- The full server address (e.g., "http://localhost:3667").
--- @return types.Result<types.HttpResponse, string> -- Success with response or failure with error message.
function ServerEndpoints.clear(address: string): types.Result<types.HttpResponse, string>
	local success, result = pcall(HttpService.RequestAsync, HttpService, {
		Method = "POST" :: "POST",
		Url = `{address}/clear`,
	})

	return wrapHttpResult(success, result)
end

--- Gets the Luau file paths within the workspace from the language server.
--- @param address -- The full server address (e.g., "http://localhost:3667").
--- @return types.Result<types.HttpResponse, string> -- Success with response or failure with error message.
function ServerEndpoints.getFilePaths(address: string): types.Result<types.HttpResponse, string>
	local success, result = pcall(HttpService.RequestAsync, HttpService, {
		Method = "GET" :: "GET",
		Url = `{address}/get-file-paths`,
	})

	return wrapHttpResult(success, result)
end

return ServerEndpoints
