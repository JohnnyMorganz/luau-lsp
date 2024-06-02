#pragma once

#include <string>
#include <unordered_map>
#include <functional>

using ErrorCallback = std::function<void(const std::string& message)>;

void registerFastFlags(std::unordered_map<std::string, std::string>& fastFlags, ErrorCallback onError, ErrorCallback onWarning);

/// Register FFlags but emit errors straight to stderr
void registerFastFlagsCLI(std::unordered_map<std::string, std::string>& fastFlags);
