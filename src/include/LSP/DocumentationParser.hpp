#pragma once
#include <optional>
#include <filesystem>
#include "Luau/Documentation.h"
#include "Luau/Module.h"
#include "Luau/Location.h"
#include "nlohmann/json.hpp"
#include "LSP/Utils.hpp"
#include "LSP/Client.hpp"

using json = nlohmann::json;

Luau::FunctionParameterDocumentation parseDocumentationParameter(const json& j);
void parseDocumentation(
    std::optional<std::filesystem::path> documentationFile, Luau::DocumentationDatabase& database, std::shared_ptr<Client> client);

/// Returns a markdown string of the provided documentation
std::string printDocumentation(const Luau::DocumentationDatabase& database, const Luau::DocumentationSymbol& symbol);

/// Get comments attached to a node (given the node's location)
std::vector<Luau::Comment> getCommentLocations(const Luau::SourceModule* module, const Luau::Location& node);