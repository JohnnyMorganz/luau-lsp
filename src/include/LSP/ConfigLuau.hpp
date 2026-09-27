#pragma once

#include <optional>
#include <string>

#include "LSP/Uri.hpp"
#include "Plugin/SourceMapping.hpp"

namespace Luau::LanguageServer::ConfigLuau
{

constexpr const char* kEnvironmentName = "LuauConfig";
constexpr const char* kDefinitionName = "LuauConfig";
constexpr const char* kCheckFunctionName = "__luau_lsp_config_check";

bool isConfigLuauFile(const Uri& uri);

const std::string& getDefinitions();

std::optional<Plugin::TransformResult> transformSourceForAnalysis(const std::string& source);

} // namespace Luau::LanguageServer::ConfigLuau
