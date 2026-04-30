--!strict

--- Debounce utility for rate-limiting function calls.
--- Delays callback execution until a period of inactivity.
---
--- Usage:
--- ```lua
--- local debounce = Debounce.new(0.5, function()
---     print("Called after 0.5s of inactivity")
--- end)
--- debounce:call() -- Starts/resets the timer
--- debounce:cancel() -- Cancels pending execution
--- ```

local Debounce = {}
Debounce.__index = Debounce

--- Internal data structure for a Debounce instance.
type DebounceData = {
	--- The delay in seconds before the callback is invoked.
	delaySeconds: number,
	--- The function to call after the delay.
	callback: () -> (),
	--- The currently pending task, if any.
	pendingTask: thread?,
}

--- A debounce controller that delays function execution.
export type Debounce = typeof(setmetatable({} :: DebounceData, Debounce))

--- Creates a new Debounce instance.
--- @param delaySeconds -- The delay in seconds before invoking the callback.
--- @param callback -- The function to call after the delay period.
--- @return Debounce -- A new Debounce instance.
function Debounce.new(delaySeconds: number, callback: () -> ()): Debounce
	local self = {
		delaySeconds = delaySeconds,
		callback = callback,
		pendingTask = nil,
	}
	return setmetatable(self, Debounce)
end

--- Schedules the callback to run after the delay.
--- If called again before the delay expires, the timer is reset.
--- @param self -- The Debounce instance.
--- @return thread -- The scheduled task thread.
function Debounce.call(self: Debounce): thread
	self:cancel()

	local pendingTask = task.delay(self.delaySeconds, function()
		self.pendingTask = nil
		self.callback()
	end)
	self.pendingTask = pendingTask
	return pendingTask
end

--- Cancels any pending callback execution.
--- @param self -- The Debounce instance.
function Debounce.cancel(self: Debounce): ()
	if self.pendingTask then
		task.cancel(self.pendingTask)
		self.pendingTask = nil
	end
end

return Debounce
