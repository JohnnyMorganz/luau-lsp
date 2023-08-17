#pragma once

#include <optional>
#include <string>
#include <variant>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

// We create our own macro with special to_json support for std::optional / nullptr
// Note: instead of converting nullptr to a JSON null, this macro omits the field completely (similar to undefined)
// WARNING: explicit nulls will be lost! If nulls are necessary (and no undefineds), then use the standard macro
#define NLOHMANN_JSON_TO_OPTIONAL(v1) \
    { \
        json val = nlohmann_json_t.v1; \
        if (val != nullptr) \
            nlohmann_json_j[#v1] = val; \
    }; // NOLINT(...)
#define NLOHMANN_DEFINE_OPTIONAL(Type, ...) \
    inline void to_json(nlohmann::json& nlohmann_json_j, const Type& nlohmann_json_t) \
    { \
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO_OPTIONAL, __VA_ARGS__)) \
    } \
    inline void from_json(const nlohmann::json& nlohmann_json_j, Type& nlohmann_json_t) \
    { \
        Type nlohmann_json_default_obj; \
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM_WITH_DEFAULT, __VA_ARGS__)) \
    } // NOLINT(...)

// Define serializer/deserializer for std::optional and std::variant
namespace nlohmann
{
template<typename T>
struct adl_serializer<std::optional<T>>
{
    static void to_json(json& j, const std::optional<T>& opt)
    {
        if (opt == std::nullopt)
            j = nullptr;
        else
            j = *opt;
    }

    static void from_json(const json& j, std::optional<T>& opt)
    {
        if (j.is_null())
            opt = std::nullopt;
        else
            opt = j.get<T>();
    }
};

// string | int is a common variant, so we will just special case it
template<>
struct adl_serializer<std::variant<std::string, int>>
{
    static void to_json(json& j, const std::variant<std::string, int>& data)
    {
        if (auto str = std::get_if<std::string>(&data))
        {
            j = *str;
        }
        else if (auto num = std::get_if<int>(&data))
        {
            j = *num;
        }
    }

    static void from_json(const json& j, std::variant<std::string, int>& data)
    {
        // TODO: handle nicely?
        assert(j.is_string() || j.is_number());

        if (j.is_string())
            data = j.get<std::string>();
        else if (j.is_number())
            data = j.get<int>();
    }
};

// Same for int | bool
template<>
struct adl_serializer<std::variant<int, bool>>
{
    static void to_json(json& j, const std::variant<int, bool>& data)
    {
        if (auto str = std::get_if<bool>(&data))
        {
            j = *str;
        }
        else if (auto num = std::get_if<int>(&data))
        {
            j = *num;
        }
    }

    static void from_json(const json& j, std::variant<int, bool>& data)
    {
        // TODO: handle nicely?
        assert(j.is_boolean() || j.is_number());

        if (j.is_boolean())
            data = j.get<bool>();
        else if (j.is_number())
            data = j.get<int>();
    }
};

template<>
struct adl_serializer<std::variant<std::string, std::vector<size_t>>>
{
    static void to_json(json& j, const std::variant<std::string, std::vector<size_t>>& data)
    {
        if (auto str = std::get_if<std::string>(&data))
        {
            j = *str;
        }
        else if (auto vec = std::get_if<std::vector<size_t>>(&data))
        {
            j = *vec;
        }
    }

    static void from_json(const json& j, std::variant<std::string, std::vector<size_t>>& data)
    {
        // TODO: handle nicely?
        assert(j.is_boolean() || j.is_array());

        if (j.is_boolean())
            data = j.get<std::string>();
        else if (j.is_array())
            data = j.get<std::vector<size_t>>();
    }
};
} // namespace nlohmann

namespace lsp
{
using LSPAny = json;

enum struct ErrorCode
{
    // Defined by JSON-RPC
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,

    /**
     * Error code indicating that a server received a notification or
     * request before the server has received the `initialize` request.
     */
    ServerNotInitialized = -32002,
    UnknownErrorCode = -32001,
    /**
     * A request failed but it was syntactically correct, e.g the
     * method name was known and the parameters were valid. The error
     * message should contain human readable information about why
     * the request failed.
     *
     * @since 3.17.0
     */
    RequestFailed = -32803,
    /**
     * The server cancelled the request. This error code should
     * only be used for requests that explicitly support being
     * server cancellable.
     *
     * @since 3.17.0
     */
    ServerCancelled = -32802,
    /**
     * The server detected that the content of a document got
     * modified outside normal conditions. A server should
     * NOT send this error code if it detects a content change
     * in it unprocessed messages. The result even computed
     * on an older state might still be useful for the client.
     *
     * If a client decides that a result is not of any use anymore
     * the client should cancel the request.
     */
    ContentModified = -32801,
    /**
     * The client has canceled a request and a server as detected
     * the cancel.
     */
    RequestCancelled = -32800,
};

using ProgressToken = std::variant<std::string, int>;

// TODO: make this generic over value
struct ProgressParams
{
    /**
     * The progress token provided by the client or server.
     */
    ProgressToken token = "";
    /**
     * The progress data.
     */
    LSPAny value = nullptr;
};
NLOHMANN_DEFINE_OPTIONAL(ProgressParams, token, value)

} // namespace lsp