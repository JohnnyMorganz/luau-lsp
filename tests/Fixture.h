// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/Config.h"
#include "Luau/FileResolver.h"
#include "Luau/Frontend.h"
#include "Luau/IostreamHelpers.h"
#include "Luau/Linter.h"
#include "Luau/Location.h"
#include "Luau/ModuleResolver.h"
#include "Luau/Scope.h"
#include "Luau/ToString.h"
#include "Luau/TypeInfer.h"
#include "Luau/Type.h"

#include "LSP/Workspace.hpp"

#include <iostream>
#include <string>
#include <unordered_map>

#include <optional>

struct Fixture
{
    std::unique_ptr<Luau::SourceModule> sourceModule;

    std::shared_ptr<Client> client;
    WorkspaceFolder workspace;

    explicit Fixture();
    ~Fixture();

    Uri newDocument(const std::string& name, const std::string& source);

    Luau::AstStatBlock* parse(const std::string& source, const Luau::ParseOptions& parseOptions = {});
    Luau::LoadDefinitionFileResult loadDefinition(const std::string& source, bool forAutocomplete = false);

    // Single file operations
    Luau::CheckResult check(Luau::Mode mode, std::string source);
    Luau::CheckResult check(const std::string& source);
    Luau::ModulePtr getMainModule();
    Luau::SourceModule* getMainSourceModule();

    std::optional<Luau::TypeId> getType(const std::string& name);
    Luau::TypeId requireType(const std::string& name);

    std::vector<std::string> getComments(const Luau::Location& node);
};
