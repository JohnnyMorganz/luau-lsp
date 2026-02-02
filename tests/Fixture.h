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
#include "TempDir.h"
#include "ScopedFlags.h"

#include <iostream>
#include <string>
#include <unordered_map>

#include <optional>

#include "TestClient.h"

LUAU_FASTFLAG(LuauSolverV2)

// TODO: the rest should be part of this namespace...
namespace Luau::LanguageServer
{
ClientConfiguration defaultTestClientConfiguration();
Uri newDocument(WorkspaceFolder& workspace, const std::string& name, const std::string& source);
} // namespace Luau::LanguageServer

struct Fixture
{
    std::unique_ptr<Luau::SourceModule> sourceModule;
    std::unique_ptr<TestClient> client;
    TempDir tempDir; // Must be declared before workspace since workspace uses tempDir.path()
    WorkspaceFolder workspace;

    explicit Fixture();
    ~Fixture();

    Uri newDocument(const std::string& name, const std::string& source);
    void registerDocumentForVirtualPath(const Uri& uri, const Luau::ModuleName& virtualPath);
    void updateDocument(const Uri& uri, const std::string& newSource);

    Luau::AstStatBlock* parse(const std::string& source, const Luau::ParseOptions& parseOptions = {});
    Luau::LoadDefinitionFileResult loadDefinition(const std::string& packageName, const std::string& source, bool forAutocomplete = false);
    void loadSourcemap(const std::string& source);
    void loadLuaurc(const std::string& source);
    SourceNode* getRootSourceNode();

    // Single file operations
    Luau::CheckResult check(Luau::Mode mode, std::string source);
    Luau::CheckResult check(const std::string& source);
    Luau::ModulePtr getModule(const Luau::ModuleName& moduleName);
    Luau::ModulePtr getMainModule();
    Luau::SourceModule* getMainSourceModule();

    std::optional<Luau::TypeId> getType(Luau::ModulePtr module, const std::string& name);
    Luau::TypeId requireType(Luau::ModulePtr module, const std::string& name);

    std::optional<Luau::TypeId> getType(const std::string& name)
    {
        return getType(getMainModule(), name);
    }
    Luau::TypeId requireType(const std::string& name)
    {
        return requireType(getMainModule(), name);
    }

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

/// Apply a set of text edits to a source string and return the result
std::string applyEdit(const std::string& source, const std::vector<lsp::TextEdit>& edits);

/// Remove common leading whitespace from each line (like Python's textwrap.dedent)
std::string dedent(std::string source);

/// Enables the new Luau type solver (LuauSolverV2) for the duration of the current scope.
/// This macro sets both the FFlag and updates the Frontend's solver mode, which is necessary
/// because the Frontend caches the solver mode at construction time.
#define ENABLE_NEW_SOLVER() \
    ScopedFastFlag _sff_new_solver_{FFlag::LuauSolverV2, true}; \
    workspace.frontend.setLuauSolverMode(Luau::SolverMode::New)
