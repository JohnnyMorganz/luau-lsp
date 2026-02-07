#pragma once
#include "Luau/Location.h"
#include <string>
#include <vector>

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
    std::string languageId;
};

// Error returned from plugin execution
struct PluginError
{
    std::string message;
    std::string pluginPath;
};

} // namespace Luau::LanguageServer::Plugin
