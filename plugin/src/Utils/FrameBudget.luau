--!strict

--- Spreads long-running work over multiple frames so that Studio stays responsive.
---
--- Usage:
--- ```lua
--- local budget = FrameBudget.new(0.008)
--- for _, item in items do
---     budget:step() -- Yields until the next frame once 8ms of work has been done
---     process(item)
--- end
--- ```

local FrameBudget = {}
FrameBudget.__index = FrameBudget

--- Internal data structure for a FrameBudget instance.
type FrameBudgetData = {
	--- The amount of work, in seconds, to do in a frame before yielding.
	_seconds: number,
	--- The time at which the current frame's budget is used up.
	_deadline: number,
}

--- Tracks how much work has been done in the current frame.
export type FrameBudget = typeof(setmetatable({} :: FrameBudgetData, FrameBudget))

--- Creates a new FrameBudget, starting the current frame's budget immediately.
--- @param seconds -- The amount of work, in seconds, to do in a frame before yielding.
--- @return FrameBudget -- A new FrameBudget.
function FrameBudget.new(seconds: number): FrameBudget
	local self = {
		_seconds = seconds,
		_deadline = os.clock() + seconds,
	}
	return setmetatable(self, FrameBudget)
end

--- Yields until the next frame if the current frame's budget has been used up.
--- @param self -- The FrameBudget instance.
--- @return boolean -- True if this call yielded.
function FrameBudget.step(self: FrameBudget): boolean
	if os.clock() < self._deadline then
		return false
	end

	task.wait()
	self._deadline = os.clock() + self._seconds
	return true
end

return FrameBudget
