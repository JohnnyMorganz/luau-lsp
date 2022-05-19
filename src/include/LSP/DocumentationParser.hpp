#pragma once
#include <optional>
#include <filesystem>
#include "Luau/Documentation.h"
#include "nlohmann/json.hpp"
#include "LSP/Utils.hpp"
#include "LSP/Client.hpp"

using json = nlohmann::json;

Luau::FunctionParameterDocumentation parseDocumentationParameter(const json& j);
void parseDocumentation(
    std::optional<std::filesystem::path> documentationFile, Luau::DocumentationDatabase& database, std::shared_ptr<Client> client);

/// Returns a markdown string of the provided documentation
std::string printDocumentation(const Luau::DocumentationDatabase& database, const Luau::DocumentationSymbol& symbol);