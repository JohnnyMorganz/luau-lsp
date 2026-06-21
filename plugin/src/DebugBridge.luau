--!strict

-- DebugBridge: bridges Roblox Studio's ScriptDebuggerService to a luau-lsp DAP
-- server via WebSocket.  Run DebugBridge.start() in play mode only.
--
-- NOTE: ScriptDebuggerService.OnStopped does not support yielding, so this
-- bridge captures the full snapshot (call stack + variables) synchronously
-- inside OnStopped and immediately returns Resume.  The DAP server caches that
-- snapshot and serves it to the editor's stackTrace/variables requests.
-- Interactive step-debugging (next/stepIn/stepOut) is not possible until
-- Roblox adds yielding support to OnStopped.

local HttpService = game:GetService("HttpService")
local LogService = game:GetService("LogService")
local ScriptDebuggerService = game:GetService("ScriptDebuggerService")

local DAP_WS_PORT = 7868

-- ── Path helpers ──────────────────────────────────────────────────────────────

-- Build a virtual path for an Instance: "game/ServiceName/Child/..."
local function getVirtualPath(instance: Instance): string
	local parts: { string } = {}
	local current: Instance? = instance
	while current ~= nil and current ~= game do
		table.insert(parts, 1, current.Name)
		current = current.Parent
	end
	table.insert(parts, 1, "game")
	return table.concat(parts, "/")
end

-- Navigate from a virtual path like "game/ServerScriptService/Foo" to an Instance
local function findScript(virtualPath: string): LuaSourceContainer?
	local parts = string.split(virtualPath, "/")
	if #parts < 2 then
		return nil
	end

	local current: Instance = game
	for i, part in parts do
		if i == 1 then
			if part ~= "game" then
				return nil
			end
		elseif i == 2 then
			-- Use GetService for top-level services
			local ok, svc = pcall(game.GetService, game, part)
			if ok and svc then
				current = svc :: Instance
			else
				local child = game:FindFirstChild(part)
				if not child then
					return nil
				end
				current = child
			end
		else
			local child = current:FindFirstChild(part)
			if not child then
				return nil
			end
			current = child
		end
	end

	if current:IsA("LuaSourceContainer") then
		return current :: LuaSourceContainer
	end
	return nil
end

-- ── Module ────────────────────────────────────────────────────────────────────

local DebugBridge = {}

function DebugBridge.start()
	local ws
	do
		local ok, result = pcall(HttpService.ConnectAsync, HttpService, "ws://localhost:" .. DAP_WS_PORT)
		if not ok or not result then
			-- DAP server not running; skip silently
			return
		end
		ws = result
	end

	local connections: { RBXScriptConnection } = {}

	local function send(data: { [string]: any })
		local ok, encoded = pcall(HttpService.JSONEncode, HttpService, data)
		if ok then
			pcall(ws.Send, ws, encoded)
		end
	end

	-- ── Breakpoint management ──────────────────────────────────────────────────

	-- Track breakpoints per script so we can clear them before re-setting
	local activeBreakpoints: { [string]: { number } } = {} -- virtualPath → [lines]

	local function handleSetBreakpoints(data: { [string]: any }, requestId: number)
		local scriptPath: string = data.scriptPath or ""
		local bpList: { any } = data.breakpoints or {}

		-- Clear previous breakpoints for this script
		if activeBreakpoints[scriptPath] then
			local script = findScript(scriptPath)
			if script then
				for _, line in activeBreakpoints[scriptPath] do
					pcall(ScriptDebuggerService.RemoveBreakpoint, ScriptDebuggerService, script, line)
				end
			end
		end
		activeBreakpoints[scriptPath] = {}

		local script = findScript(scriptPath)
		local results: { { [string]: any } } = {}

		if script then
			for _, bp in bpList do
				local bpArgs: { [string]: any } = { Line = bp.line }
				if bp.condition then
					bpArgs.Condition = bp.condition
				end
				if bp.logMessage then
					bpArgs.LogMessage = bp.logMessage
					bpArgs.ContinueExecution = true
				end

				local ok, bpResult = pcall(
					ScriptDebuggerService.AddBreakpoint,
					ScriptDebuggerService,
					script,
					bpArgs
				)
				if ok and bpResult then
					table.insert(results, {
						verified = bpResult.Verified,
						line = bpResult.Line,
						message = bpResult.Message,
					})
					table.insert(activeBreakpoints[scriptPath], bp.line)
				else
					table.insert(results, { verified = false, line = bp.line })
				end
			end
		else
			for _, bp in bpList do
				table.insert(results, { verified = false, line = bp.line })
			end
		end

		send({ response = "setBreakpoints", requestId = requestId, breakpoints = results })
	end

	-- ── OnStopped ─────────────────────────────────────────────────────────────
	-- ScriptDebuggerService.OnStopped is a function property (not a signal).
	-- It cannot yield; it must return {steppedType, threadId} synchronously.
	-- We capture the full snapshot here so the DAP server can serve it later.

	ScriptDebuggerService.OnStopped = function(info)
		local threadIds: { number } = info.ThreadIds or {}
		local threadId: number = #threadIds > 0 and threadIds[1] or 0

		-- Capture call stack synchronously
		local frames: { { [string]: any } } = {}
		local variableMap: { [string]: { { [string]: any } } } = {}

		local ok, traceResult = pcall(
			ScriptDebuggerService.GetStackTrace,
			ScriptDebuggerService,
			threadId
		)
		if ok and traceResult and traceResult.Frames then
			for _, frame in traceResult.Frames do
				local scriptPath = ""
				-- ScriptPath is a string virtual path from ScriptDebuggerService
				if type(frame.ScriptPath) == "string" then
					scriptPath = frame.ScriptPath
				elseif typeof(frame.ScriptPath) == "Instance" then
					scriptPath = getVirtualPath(frame.ScriptPath :: Instance)
				end

				table.insert(frames, {
					id = frame.Id or 0,
					name = frame.Name or "?",
					scriptPath = scriptPath,
					line = frame.Line or 0,
					column = 0,
				})

				-- Capture root variables for this frame
				local ok2, vars = pcall(
					ScriptDebuggerService.GetRootVariables,
					ScriptDebuggerService,
					frame.Id or 0
				)
				if ok2 and vars then
					local varList: { { [string]: any } } = {}
					for _, v in vars do
						table.insert(varList, {
							name = v.Name or "?",
							value = v.Value or "nil",
							type = v.Type,
							variablesReference = v.VariablesReference or 0,
						})
					end
					variableMap[tostring(frame.Id or 0)] = varList
				end
			end
		end

		-- Send extended stopped event with full snapshot
		local msg: { [string]: any } = {
			event = "stopped",
			reason = tostring(info.Reason or "breakpoint"),
			threadIds = threadIds,
			snapshot = {
				threadId = threadId,
				frames = frames,
				variables = variableMap,
			},
		}
		if info.ExceptionText then
			msg.exceptionText = info.ExceptionText
		end
		send(msg)

		-- Must return synchronously; game will resume immediately.
		-- Step-debugging requires Roblox to add yielding support to OnStopped.
		return { steppedType = Enum.DebuggerResumeType.Resume, threadId = threadId }
	end

	-- ── Resumed event ──────────────────────────────────────────────────────────
	if ScriptDebuggerService.Resumed then
		table.insert(
			connections,
			ScriptDebuggerService.Resumed:Connect(function(threadIds: { number })
				send({ event = "continued", threadId = #threadIds > 0 and threadIds[1] or 0 })
			end)
		)
	end

	-- ── Output capture ─────────────────────────────────────────────────────────
	table.insert(
		connections,
		LogService.MessageOut:Connect(function(message: string, messageType: Enum.MessageType)
			local category = "stdout"
			if messageType == Enum.MessageType.MessageWarning then
				category = "stderr"
			elseif messageType == Enum.MessageType.MessageError then
				category = "stderr"
			end
			send({ event = "output", output = message .. "\n", category = category })
		end)
	)

	-- ── Incoming command handler ───────────────────────────────────────────────
	table.insert(
		connections,
		ws.OnMessage:Connect(function(rawMsg: string)
			local ok, data = pcall(HttpService.JSONDecode, HttpService, rawMsg)
			if not ok then
				return
			end

			local command: string = data.command or ""
			local requestId: number = data.requestId or 0

			if command == "setBreakpoints" then
				handleSetBreakpoints(data, requestId)

			elseif command == "setExceptionBreakpoints" then
				local breakOnAll = false
				for _, f in (data.filters or {}) do
					if f == "all" then
						breakOnAll = true
					end
				end
				pcall(
					ScriptDebuggerService.SetExceptionBreakMode,
					ScriptDebuggerService,
					if breakOnAll
						then Enum.DebugBreakModeType.Always
						else Enum.DebugBreakModeType.Never
				)

			elseif command == "pause" then
				pcall(ScriptDebuggerService.Pause, ScriptDebuggerService)
			end
			-- continue/next/stepIn/stepOut are handled on the C++ bridge side
			-- (the game already resumed when OnStopped returned)
		end)
	)

	-- ── Cleanup ────────────────────────────────────────────────────────────────
	table.insert(
		connections,
		ws.OnClose:Connect(function()
			for _, conn in connections do
				conn:Disconnect()
			end
			connections = {}
			ScriptDebuggerService.OnStopped = nil
		end)
	)

	-- Signal bridge is ready
	send({ event = "ready" })
end

return DebugBridge
