#pragma once
#include <optional>
#include "Luau/AstQuery.h"
#include "Luau/Frontend.h"
#include "Luau/Scope.h"
#include "Luau/ToString.h"
#include "LSP/WorkspaceFileResolver.hpp"
#include "Protocol/Structures.hpp"
#include "Protocol/Diagnostics.hpp"

namespace types
{
std::optional<Luau::TypeId> getTypeIdForClass(const Luau::ScopePtr& globalScope, std::optional<std::string> className);
std::optional<std::string> getTypeName(Luau::TypeId typeId);

bool isMetamethod(const Luau::Name& name);

void registerInstanceTypes(Luau::Frontend& frontend, const Luau::GlobalTypes& globals, Luau::TypeArena& arena,
    const WorkspaceFileResolver& fileResolver, bool expressiveTypes);
Luau::LoadDefinitionFileResult registerDefinitions(
    Luau::Frontend& frontend, Luau::GlobalTypes& globals, const std::string& definitions, bool typeCheckForAutocomplete = false);

using NameOrExpr = std::variant<std::string, Luau::AstExpr*>;

// Converts a FTV and function call to a nice string
// In the format "function NAME(args): ret"
struct ToStringNamedFunctionOpts
{
    bool hideTableKind = false;
    bool multiline = false;
};

std::string toStringNamedFunction(const Luau::ModulePtr& module, const Luau::FunctionType* ftv, const NameOrExpr nameOrFuncExpr,
    std::optional<Luau::ScopePtr> scope = std::nullopt, const ToStringNamedFunctionOpts& opts = {});

std::string toStringReturnType(Luau::TypePackId retTypes, Luau::ToStringOptions options = {});
Luau::ToStringResult toStringReturnTypeDetailed(Luau::TypePackId retTypes, Luau::ToStringOptions options = {});

// Duplicated from Luau/TypeInfer.h, since its static
std::optional<Luau::AstExpr*> matchRequire(const Luau::AstExprCall& call);

} // namespace types

// TODO: should upstream this
Luau::AstNode* findNodeOrTypeAtPosition(const Luau::SourceModule& source, Luau::Position pos);
Luau::AstNode* findNodeOrTypeAtPositionClosed(const Luau::SourceModule& source, Luau::Position pos);
Luau::ExprOrLocal findExprOrLocalAtPositionClosed(const Luau::SourceModule& source, Luau::Position pos);
std::vector<Luau::Location> findSymbolReferences(const Luau::SourceModule& source, Luau::Symbol symbol);
std::vector<Luau::Location> findTypeReferences(const Luau::SourceModule& source, const Luau::Name& typeName, std::optional<const Luau::Name> prefix);

std::optional<Luau::Location> getLocation(Luau::TypeId type);

std::optional<Luau::Location> lookupTypeLocation(const Luau::Scope& deepScope, const Luau::Name& name);
std::optional<Luau::Property> lookupProp(const Luau::TypeId& parentType, const Luau::Name& name);
std::optional<Luau::ModuleName> lookupImportedModule(const Luau::Scope& deepScope, const Luau::Name& name);

// Converts a UTF-8 position to a UTF-16 position, using the provided text document if available
// NOTE: if the text document doesn't exist, we perform no conversion, so the positioning may be
// incorrect
lsp::Position toUTF16(const TextDocument* textDocument, const Luau::Position& position);

lsp::Diagnostic createTypeErrorDiagnostic(const Luau::TypeError& error, Luau::FileResolver* fileResolver, const TextDocument* textDocument = nullptr);
lsp::Diagnostic createLintDiagnostic(const Luau::LintWarning& lint, const TextDocument* textDocument = nullptr);
lsp::Diagnostic createParseErrorDiagnostic(const Luau::ParseError& error, const TextDocument* textDocument = nullptr);

bool isGetService(const Luau::AstExpr* expr);
bool isRequire(const Luau::AstExpr* expr);

struct FindImportsVisitor : public Luau::AstVisitor
{
private:
    std::optional<size_t> previousRequireLine = std::nullopt;

public:
    std::optional<size_t> firstServiceDefinitionLine = std::nullopt;
    std::optional<size_t> lastServiceDefinitionLine = std::nullopt;
    std::map<std::string, Luau::AstStatLocal*> serviceLineMap{};
    std::optional<size_t> firstRequireLine = std::nullopt;
    std::vector<std::map<std::string, Luau::AstStatLocal*>> requiresMap{{}};

    size_t findBestLineForService(const std::string& serviceName, size_t minimumLineNumber)
    {
        if (firstServiceDefinitionLine)
            minimumLineNumber = *firstServiceDefinitionLine > minimumLineNumber ? *firstServiceDefinitionLine : minimumLineNumber;

        size_t lineNumber = minimumLineNumber;
        for (auto& [definedService, stat] : serviceLineMap)
        {
            auto location = stat->location.end.line;
            if (definedService < serviceName && location >= lineNumber)
                lineNumber = location + 1;
        }
        return lineNumber;
    }

    bool containsRequire(const std::string& module)
    {
        for (const auto& map : requiresMap)
            if (contains(map, module))
                return true;
        return false;
    }

    bool visit(Luau::AstStatLocal* local) override
    {
        if (local->vars.size != 1 || local->values.size != 1)
            return false;

        auto localName = local->vars.data[0];
        auto expr = local->values.data[0];

        if (!localName || !expr)
            return false;

        auto line = expr->location.end.line;

        if (isGetService(expr))
        {
            firstServiceDefinitionLine =
                !firstServiceDefinitionLine.has_value() || firstServiceDefinitionLine.value() >= line ? line : firstServiceDefinitionLine.value();
            lastServiceDefinitionLine =
                !lastServiceDefinitionLine.has_value() || lastServiceDefinitionLine.value() <= line ? line : lastServiceDefinitionLine.value();
            serviceLineMap.emplace(std::string(localName->name.value), local);
        }
        else if (isRequire(expr))
        {
            firstRequireLine = !firstRequireLine.has_value() || firstRequireLine.value() >= line ? line : firstRequireLine.value();

            // If the requires are too many lines away, treat it as a new group
            if (previousRequireLine && line - previousRequireLine.value() > 1)
                requiresMap.push_back({}); // Construct a new group

            requiresMap.back().emplace(std::string(localName->name.value), local);
            previousRequireLine = line;
        }

        return false;
    }

    bool visit(Luau::AstStatBlock* block) override
    {
        for (Luau::AstStat* stat : block->body)
        {
            stat->visit(this);
        }

        return false;
    }
};