--!strict

--- Logging utility for the Luau Language Server plugin.
--- Provides leveled logging with configurable output filtering.
---
--- Usage:
--- ```lua
--- Log.setLevel(Log.Level.DEBUG)
--- Log.info("Connected to server")
--- Log.debug("Processing", itemCount, "items")
--- ```

local Log = {}

--- Numeric log level constants for filtering messages.
--- Lower values are more verbose.
Log.Level = {
	DEBUG = 1,
	INFO = 2,
	WARN = 3,
	ERROR = 4,
}

--- Prefix prepended to all log messages for identification.
Log._prefix = "[Luau LSP] "

--- Maps log levels to their output functions.
Log._levelMethod = {
	[Log.Level.DEBUG] = function(...: any)
		print(...)
	end,
	[Log.Level.INFO] = function(...: any)
		print(...)
	end,
	[Log.Level.WARN] = function(...: any)
		warn(...)
	end,
	[Log.Level.ERROR] = function(...: any)
		error(...)
	end,
}

--- Maps log levels to their prefix strings.
Log._levelPrefix = {
	[Log.Level.DEBUG] = "DEBUG:",
	[Log.Level.INFO] = "INFO: ",
	[Log.Level.WARN] = "WARN: ",
	[Log.Level.ERROR] = "ERROR:",
}

--- The current minimum log level. Messages below this level are suppressed.
Log._currentLevel = Log.Level.INFO

--- Sets the minimum log level for message output.
--- Messages with a level below this threshold will be suppressed.
--- @param level -- The numeric log level (use Log.Level constants).
function Log.setLevel(level: number): ()
	Log._currentLevel = math.clamp(level, Log.Level.DEBUG, Log.Level.ERROR)
end

--- Internal logging function that checks the level and outputs the message.
--- @param level -- The numeric log level of this message.
--- @param ... -- The values to log.
function Log._log(level: number, ...: any): ()
	if level < Log._currentLevel then
		return
	end

	local method = Log._levelMethod[level] or Log._levelMethod[Log.Level.INFO]
	local levelPrefix = Log._levelPrefix[level] or Log._levelPrefix[Log.Level.INFO]
	method(Log._prefix .. levelPrefix, ...)
end

--- Logs a debug message. Only shown when log level is DEBUG.
--- @param ... -- The values to log.
function Log.debug(...): ()
	Log._log(Log.Level.DEBUG, ...)
end

--- Logs an informational message. Shown when log level is INFO or lower.
--- @param ... -- The values to log.
function Log.info(...): ()
	Log._log(Log.Level.INFO, ...)
end

--- Logs a warning message. Shown when log level is WARN or lower.
--- @param ... -- The values to log.
function Log.warn(...): ()
	Log._log(Log.Level.WARN, ...)
end

--- Logs an error message and halts execution.
--- Always shown regardless of log level.
--- @param ... -- The values to include in the error message.
function Log.error(...): never
	-- In order to type this as never, we can't use _log here
	error(`{Log._prefix}{Log._levelPrefix[Log.Level.ERROR]} {table.concat({ ... }, " ")}`, 2)
end

return Log
