--!strict

--- A dead simple strictly typed signal/event utility for managing listeners.
--- Provides a reusable pattern for registering callbacks and firing events.
---
--- Usage:
--- ```lua
--- local signal = Signal.new()
--- local disconnect = signal:connect(function(value)
---     print("Received:", value)
--- end)
--- signal:fire("Hello") -- Prints: Received: Hello
--- disconnect() -- Unregisters the listener
--- ```

local Log = require(script.Parent.Log)

local Signal = {}
Signal.__index = Signal

--- Internal data structure for a Signal instance.
type SignalData<T...> = {
	--- List of registered listener callbacks.
	_listeners: { (T...) -> () },
}

--- A signal that can have listeners connected and fire events.
export type Signal<T...> = typeof(setmetatable({} :: SignalData<T...>, Signal))

--- Creates a new Signal instance.
--- @return Signal -- A new Signal ready for listeners.
function Signal.new<T...>(): Signal<T...>
	local self = {
		_listeners = {},
	}
	return setmetatable(self, Signal)
end

--- Registers a callback to be invoked when the signal fires.
--- @param self -- The Signal instance.
--- @param listener -- The callback function to register.
--- @return () -> () -- A function to unregister the listener.
function Signal.connect<T...>(self: Signal<T...>, listener: (T...) -> ()): () -> ()
	table.insert(self._listeners, listener)
	return function()
		local index = table.find(self._listeners, listener)
		if index then
			table.remove(self._listeners, index)
		end
	end
end

--- Invokes all registered listeners with the provided arguments.
--- @param self -- The Signal instance.
--- @param ... -- The arguments to pass to each listener.
function Signal.fire<T...>(self: Signal<T...>, ...: T...): ()
	for _, listener in self._listeners do
		xpcall(listener, function(err)
			Log.warn(`Error calling listener: {err}`)
		end, ...)
	end
end

--- Removes all listeners from the signal.
--- Call this when the signal is no longer needed to prevent memory leaks.
--- @param self -- The Signal instance.
function Signal.destroy<T...>(self: Signal<T...>): ()
	table.clear(self._listeners)
end

return Signal
