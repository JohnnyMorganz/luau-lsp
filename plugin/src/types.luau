--!strict

--- Type definitions for the Luau Language Server plugin.
--- This module exports common types used throughout the plugin.

--- Represents a successful result containing a value of type T.
type Ok<T> = { ok: true, value: T }

--- Represents a failed result containing an error of type E.
type Err<E> = { ok: false, error: E }

--- A discriminated union representing either success or failure.
--- Use `result.ok` to determine which variant you have.
export type Result<T, E> = Ok<T> | Err<E>

--- Represents a serialized Instance for transmission to the language server.
--- Contains the instance's name, class, and recursively encoded children.
export type EncodedInstance = {
	--- Identifies the instance in incremental changes.
	Id: number,
	--- Identifies the full DataModel that incremental changes apply to. Only set on the root instance.
	Session: string?,
	Name: string,
	ClassName: string,
	--- Omitted if empty.
	FilePaths: { string }?,
	--- Omitted if empty.
	Children: { EncodedInstance }?,
}

--- An incremental change to the DataModel previously sent to the language server.
--- Instances are referred to by their `Id`.
export type Change =
	{ Type: "Add", Parent: number, Instance: EncodedInstance }
	| { Type: "Remove", Id: number }
	| { Type: "Update", Id: number, Name: string?, Parent: number?, FilePaths: { string }? }

--- Valid log levels for the plugin's logging system.
export type LogLevel = "DEBUG" | "INFO" | "WARN" | "ERROR"

--- Configuration options for the plugin.
--- These settings are stored in a ModuleScript in TestService.
export type Settings = {
	--- The hostname of the language server (e.g., "http://localhost").
	host: string,
	--- The port number the language server is listening on.
	port: number,
	--- Whether to automatically connect when Studio starts.
	startAutomatically: boolean,
	--- Instances whose tree should be tracked for changes.
	include: { Instance },
	--- The minimum log level for messages to be displayed.
	logLevel: LogLevel,
}

--- Standard HTTP response structure returned by Roblox HttpService.
export type HttpResponse = {
	--- Whether the HTTP request was successful (status code 2xx).
	Success: boolean,
	--- The HTTP status code of the response.
	StatusCode: number,
	--- Human-readable status message (e.g., "OK", "Not Found").
	StatusMessage: string,
	--- Response headers as key-value pairs.
	Headers: { [string]: string },
	--- The response body as a string.
	Body: string,
}

return {}
