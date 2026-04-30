#pragma once

#include <optional>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"
#include "Luau/Documentation.h"
#include "Luau/Module.h"
#include "Luau/Location.h"
#include "LSP/Client.hpp"

using json = nlohmann::json;

const std::string kDocumentationBreaker = "\n----------\n";

Luau::FunctionParameterDocumentation parseDocumentationParameter(const json& j);
void parseDocumentation(const std::vector<std::string>& documentationFiles, Luau::DocumentationDatabase& database, const Client* client);

/// Returns a markdown string of the provided documentation
/// If we can't find any documentation for the given symbol, then we return nullopt
std::optional<std::string> printDocumentation(const Luau::DocumentationDatabase& database, const Luau::DocumentationSymbol& symbol);

/// Returns a markdown string of moonwave-parsed comments
std::string printMoonwaveDocumentation(const std::vector<std::string>& comments);

/// Get comments attached to a node (given the node's location)
std::vector<Luau::Comment> getCommentLocations(const Luau::SourceModule* module, const Luau::Location& node);
