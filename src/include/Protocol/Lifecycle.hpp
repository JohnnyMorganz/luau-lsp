#pragma once

#include <optional>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"
#include "Protocol/Structures.hpp"
#include "Protocol/ClientCapabilities.hpp"
#include "Protocol/ServerCapabilities.hpp"

namespace lsp
{
enum struct TraceValue
{
    Off,
    Messages,
    Verbose,
};
NLOHMANN_JSON_SERIALIZE_ENUM(TraceValue, {{TraceValue::Off, "off"}, {TraceValue::Messages, "messages"}, {TraceValue::Verbose, "verbose"}})

/**
 * General parameters to register for a capability.
 */
struct Registration
{
    /**
     * The id used to register the request. The id can be used to deregister
     * the request again.
     */
    std::string id;

    /**
     * The method / capability to register for.
     */
    std::string method;

    /**
     * Options necessary for the registration.
     */
    LSPAny registerOptions = nullptr;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Registration, id, method, registerOptions)

struct RegistrationParams
{
    std::vector<Registration> registrations{};
};
NLOHMANN_DEFINE_OPTIONAL(RegistrationParams, registrations)

/**
 * General parameters to unregister a capability.
 */
struct Unregistration
{
    /**
     * The id used to unregister the request or notification. Usually an id
     * provided during the register request.
     */
    std::string id;

    /**
     * The method / capability to unregister for.
     */
    std::string method;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Unregistration, id, method)

struct UnregistrationParams
{
    std::vector<Unregistration> unregistrations{};
};
NLOHMANN_DEFINE_OPTIONAL(UnregistrationParams, unregistrations)

struct InitializeParams
{
    struct ClientInfo
    {
        /**
         * The name of the client as defined by the client.
         */
        std::string name;
        /**
         * The client's version as defined by the client.
         */
        std::optional<std::string> version = std::nullopt;
    };

    /**
     * The process Id of the parent process that started the server. Is null if
     * the process has not been started by another process. If the parent
     * process is not alive then the server should exit (see exit notification)
     * its process.
     */
    std::optional<int> processId = std::nullopt;

    /**
     * Information about the client
     *
     * @since 3.15.0
     */
    std::optional<ClientInfo> clientInfo = std::nullopt;

    /**
     * The locale the client is currently showing the user interface
     * in. This must not necessarily be the locale of the operating
     * system.
     *
     * Uses IETF language tags as the value's syntax
     * (See https://en.wikipedia.org/wiki/IETF_language_tag)
     *
     * @since 3.16.0
     */
    std::optional<std::string> locale = std::nullopt;

    // TODO: rootPath (deprecated?)

    /**
     * The rootUri of the workspace. Is null if no
     * folder is open. If both `rootPath` and `rootUri` are set
     * `rootUri` wins.
     *
     * @deprecated in favour of `workspaceFolders`
     */
    std::optional<DocumentUri> rootUri = std::nullopt;

    /**
     * User provided initialization options.
     */
    std::optional<LSPAny> initializationOptions = std::nullopt;

    /**
     * The capabilities provided by the client (editor or tool)
     */
    ClientCapabilities capabilities;

    /**
     * The initial trace setting. If omitted trace is disabled ('off').
     */
    TraceValue trace = TraceValue::Off;

    /**
     * The workspace folders configured in the client when the server starts.
     * This property is only available if the client supports workspace folders.
     * It can be `null` if the client supports workspace folders but none are
     * configured.
     *
     * @since 3.6.0
     */
    std::optional<std::vector<WorkspaceFolder>> workspaceFolders = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(InitializeParams::ClientInfo, name, version)
NLOHMANN_DEFINE_OPTIONAL(InitializeParams, processId, clientInfo, locale, rootUri, initializationOptions, capabilities, trace, workspaceFolders)


struct InitializeResult
{
    struct ServerInfo
    {
        std::string name;
        std::optional<std::string> version = std::nullopt;
    };

    ServerCapabilities capabilities;
    std::optional<ServerInfo> serverInfo = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(InitializeResult::ServerInfo, name, version)
NLOHMANN_DEFINE_OPTIONAL(InitializeResult, capabilities, serverInfo)

struct InitializedParams
{
};
inline void from_json(const json&, InitializedParams&){};

struct SetTraceParams
{
    /**
     * The new value that should be assigned to the trace setting.
     */
    TraceValue value;
};
NLOHMANN_DEFINE_OPTIONAL(SetTraceParams, value)


} // namespace lsp
