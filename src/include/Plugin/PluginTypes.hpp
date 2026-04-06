#pragma once
#include "Luau/Location.h"
#include <string>

namespace Luau::LanguageServer::Plugin
{

// A text edit to apply to source code
// Uses Luau positions (0-indexed lines, 0-indexed UTF-8 byte columns)
struct TextEdit
{
    Luau::Location range;
    std::string newText;
};

// Context provided to plugins about the file being transformed
struct PluginContext
{
    std::string filePath;
    std::string moduleName;
};

// Error returned from plugin execution
struct PluginError
{
    std::string message;
};

} // namespace Luau::LanguageServer::Plugin
