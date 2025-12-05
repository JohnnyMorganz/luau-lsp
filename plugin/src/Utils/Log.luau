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
Log._prefix = "[Luau Language Server]:"

--- Maps log levels to their output functions.
Log._levelMethod = {
	[Log.Level.DEBUG] = print,
	[Log.Level.INFO] = print,
	[Log.Level.WARN] = warn,
	[Log.Level.ERROR] = error,
} :: { [number]: (...any) -> () }

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

	local method = Log._levelMethod[level]
	if method then
		method(Log._prefix, ...)
	else
		print(Log._prefix, ...)
	end
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
	error(`{Log._prefix} {table.concat({ ... }, " ")}`, 2)
end

return Log
