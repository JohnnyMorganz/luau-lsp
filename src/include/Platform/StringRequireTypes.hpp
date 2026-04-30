#pragma once

#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "Luau/TypeFwd.h"

namespace Luau::LanguageServer::AutoImports
{
/// Callback to compute a require path between two modules.
/// Returns (requirePath, sortText), or nullopt to skip this module.
using RequirePathComputer = std::function<
    std::optional<std::pair<std::string, const char*>>(const Luau::ModuleName& from, const Luau::ModuleName& target)>;

/// Callback to visit all candidate module names for auto-import.
using ModuleVisitor = std::function<void(const std::function<void(const Luau::ModuleName&)>&)>;
} // namespace Luau::LanguageServer::AutoImports
