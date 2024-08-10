#pragma once
#include <optional>
#include "Luau/AstQuery.h"
#include "Luau/Frontend.h"
#include "Luau/Scope.h"
#include "Luau/ToString.h"
#include "LSP/WorkspaceFileResolver.hpp"
#include "Protocol/Structures.hpp"
#include "Protocol/Diagnostics.hpp"

#include "nlohmann/json.hpp"

namespace types
{
std::optional<std::string> getTypeName(Luau::TypeId typeId);

bool isMetamethod(const Luau::Name& name);

std::optional<nlohmann::json> parseDefinitionsFileMetadata(const std::string& definitions);

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
std::pair<std::vector<Luau::Location>, std::vector<lsp::DocumentHighlightKind>> findSymbolReferencesWithKinds(
    const Luau::SourceModule& source, Luau::Symbol symbol);
std::vector<Luau::Location> findPropertyReferences(
    const Luau::SourceModule& source, const Luau::Name& property, Luau::TypeId ty, Luau::DenseHashMap<const Luau::AstExpr*, Luau::TypeId> astTypes);
std::pair<std::vector<Luau::Location>, std::vector<lsp::DocumentHighlightKind>> findPropertyReferencesWithKinds(
    const Luau::SourceModule& source, const Luau::Name& property, Luau::TypeId ty, Luau::DenseHashMap<const Luau::AstExpr*, Luau::TypeId> astTypes);
std::vector<Luau::Location> findTypeReferences(const Luau::SourceModule& source, const Luau::Name& typeName, std::optional<const Luau::Name> prefix);
std::vector<Luau::Location> findTypeParameterUsages(Luau::AstNode& node, Luau::AstName name);
std::pair<std::vector<Luau::Location>, std::vector<lsp::DocumentHighlightKind>> findTypeParameterUsagesWithKinds(
    Luau::AstNode& node, Luau::AstName name);
std::optional<Luau::AstName> findTypeReferenceName(
    Luau::Position pos, Luau::AstArray<Luau::AstGenericType> generics, Luau::AstArray<Luau::AstGenericTypePack> genericPacks);
std::optional<std::pair<Luau::AstLocal*, Luau::AstExpr*>> findClosestAncestorModuleImport(
    const Luau::SourceModule& source, const Luau::AstName name, const Luau::Position pos);

std::optional<Luau::Location> getLocation(Luau::TypeId type);

std::optional<Luau::Location> lookupTypeLocation(const Luau::Scope& deepScope, const Luau::Name& name);
std::optional<std::pair<Luau::TypeId, Luau::Property>> lookupProp(const Luau::TypeId& parentType, const Luau::Name& name);
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
bool isMethod(const Luau::FunctionType* ftv);
bool isOverloadedMethod(Luau::TypeId ty);
bool isSameTable(Luau::TypeId a, Luau::TypeId b);
bool isTypeReference(Luau::AstName name, Luau::AstArray<Luau::AstGenericType> generics, Luau::AstArray<Luau::AstGenericTypePack> genericPacks);

struct FindImportsVisitor : public Luau::AstVisitor
{
private:
    std::optional<size_t> previousRequireLine = std::nullopt;

public:
    std::optional<size_t> firstRequireLine = std::nullopt;
    std::vector<std::map<std::string, Luau::AstStatLocal*>> requiresMap{{}};

    virtual bool handleLocal(Luau::AstStatLocal* local, Luau::AstLocal* localName, Luau::AstExpr* expr, unsigned int startLine, unsigned int endLine)
    {
        return false;
    }

    [[nodiscard]] virtual size_t getMinimumRequireLine() const
    {
        return 0;
    }

    [[nodiscard]] virtual bool shouldPrependNewline(size_t lineNumber) const
    {
        return false;
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

        auto startLine = local->location.begin.line;
        auto endLine = local->location.end.line;

        if (handleLocal(local, localName, expr, startLine, endLine))
            return false;

        if (isRequire(expr))
        {
            firstRequireLine = !firstRequireLine.has_value() || firstRequireLine.value() >= startLine ? startLine : firstRequireLine.value();

            // If the requires are too many lines away, treat it as a new group
            if (previousRequireLine && startLine - previousRequireLine.value() > 1)
                requiresMap.emplace_back(); // Construct a new group

            requiresMap.back().emplace(std::string(localName->name.value), local);
            previousRequireLine = endLine;
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
