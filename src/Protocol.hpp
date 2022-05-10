#include <string>
#include <optional>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace lsp
{
enum ErrorCode
{
    // JSON RPC errors
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,

    // LSP Errors
    ServerNotInitialized = -32002,
    UnknownErrorCode = -32001,
    RequestFailed = -32803,
    ServerCancelled = -32802,
    ContentModified = -32801,
    RequestCancelled = -32800,
};

// struct Position
// {
//     unsigned int line;
//     unsigned int character;
// };

// struct Range
// {
//     Position start;
//     Position end;
// };

} // namespace lsp