#pragma once
#include <string>

namespace lsp
{
enum struct MessageType
{
    Error = 1,
    Warning = 2,
    Info = 3,
    Log = 4,
};

struct LogMessageParams
{
    MessageType type = MessageType::Error;
    std::string message;
};
NLOHMANN_DEFINE_OPTIONAL(LogMessageParams, type, message)

struct ShowMessageParams
{
    MessageType type = MessageType::Error;
    std::string message;
};
NLOHMANN_DEFINE_OPTIONAL(ShowMessageParams, type, message)
} // namespace lsp
