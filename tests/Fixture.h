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

// TODO: the rest should be part of this namespace...
namespace Luau::LanguageServer
{
ClientConfiguration defaultTestClientConfiguration();
Uri newDocument(WorkspaceFolder& workspace, const std::string& name, const std::string& source);
} // namespace Luau::LanguageServer

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
    void loadSourcemap(const std::string& source);
    SourceNodePtr getRootSourceNode();

    // Single file operations
    Luau::CheckResult check(Luau::Mode mode, std::string source);
    Luau::CheckResult check(const std::string& source);
    Luau::ModulePtr getModule(const Luau::ModuleName& moduleName);
    Luau::ModulePtr getMainModule();
    Luau::SourceModule* getMainSourceModule();

    std::optional<Luau::TypeId> getType(Luau::ModulePtr module, const std::string& name);
    Luau::TypeId requireType(Luau::ModulePtr module, const std::string& name);

    std::optional<Luau::TypeId> getType(const std::string& name) { return getType(getMainModule(), name); }
    Luau::TypeId requireType(const std::string& name) { return requireType(getMainModule(), name); }

    std::vector<std::string> getComments(const Luau::Location& node);

    void dumpErrors(std::ostream& os, const std::vector<Luau::TypeError>& errors);
    std::string getErrors(const Luau::CheckResult& cr);
};

#define LUAU_LSP_REQUIRE_ERRORS(result) \
    do \
    { \
        auto&& r = (result); \
        REQUIRE(!r.errors.empty()); \
    } while (false)

#define LUAU_LSP_REQUIRE_ERROR_COUNT(count, result) \
    do \
    { \
        auto&& r = (result); \
        REQUIRE_MESSAGE(count == r.errors.size(), getErrors(r)); \
    } while (false)

#define LUAU_LSP_REQUIRE_NO_ERRORS(result) LUAU_LSP_REQUIRE_ERROR_COUNT(0, result)

std::pair<std::string, lsp::Position> sourceWithMarker(std::string source);