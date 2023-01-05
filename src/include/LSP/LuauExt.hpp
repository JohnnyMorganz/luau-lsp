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

Luau::TypeId makeLazyInstanceType(const Luau::TypeChecker& typeChecker, Luau::TypeArena& arena, const Luau::ScopePtr& globalScope,
    const SourceNodePtr& node, std::optional<Luau::TypeId> parent, std::optional<Luau::TypeId> baseClass = std::nullopt);

// Magic function for `Instance:IsA("ClassName")` predicate
std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionInstanceIsA(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId> exprResult);

// Magic function for `instance:Clone()`, so that we return the exact subclass that `instance` is, rather than just a generic Instance
std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionInstanceClone(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId> exprResult);

// Magic function for `Instance:FindFirstChildWhichIsA("ClassName")` and friends
std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionFindFirstXWhichIsA(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId> exprResult);

void registerInstanceTypes(Luau::TypeChecker& typeChecker, Luau::TypeArena& arena, const WorkspaceFileResolver& fileResolver, bool expressiveTypes);
Luau::LoadDefinitionFileResult registerDefinitions(Luau::TypeChecker& typeChecker, const std::string& definitions);
Luau::LoadDefinitionFileResult registerDefinitions(Luau::TypeChecker& typeChecker, const std::filesystem::path& definitionsFile);

using NameOrExpr = std::variant<std::string, Luau::AstExpr*>;

// Converts a FTV and function call to a nice string
// In the format "function NAME(args): ret"
struct ToStringNamedFunctionOpts
{
    bool hideTableKind = false;
    bool multiline = false;
};

std::string toStringNamedFunction(Luau::ModulePtr module, const Luau::FunctionType* ftv, const NameOrExpr nameOrFuncExpr,
    std::optional<Luau::ScopePtr> scope = std::nullopt, ToStringNamedFunctionOpts opts = {});

std::string toStringReturnType(Luau::TypePackId retTypes, Luau::ToStringOptions options = {});
Luau::ToStringResult toStringReturnTypeDetailed(Luau::TypePackId retTypes, Luau::ToStringOptions options = {});

// Duplicated from Luau/TypeInfer.h, since its static
std::optional<Luau::AstExpr*> matchRequire(const Luau::AstExprCall& call);

} // namespace types

// TODO: should upstream this
struct FindNodeType : public Luau::AstVisitor
{
    const Luau::Position pos;
    const Luau::Position documentEnd;
    Luau::AstNode* best = nullptr;

    explicit FindNodeType(Luau::Position pos, Luau::Position documentEnd)
        : pos(pos)
        , documentEnd(documentEnd)
    {
    }

    bool visit(Luau::AstNode* node) override
    {
        if (node->location.contains(pos))
        {
            best = node;
            return true;
        }

        // Edge case: If we ask for the node at the position that is the very end of the document
        // return the innermost AST element that ends at that position.

        if (node->location.end == documentEnd && pos >= documentEnd)
        {
            best = node;
            return true;
        }

        return false;
    }

    virtual bool visit(class Luau::AstType* node) override
    {
        return visit(static_cast<Luau::AstNode*>(node));
    }

    bool visit(Luau::AstStatBlock* block) override
    {
        visit(static_cast<Luau::AstNode*>(block));

        for (Luau::AstStat* stat : block->body)
        {
            if (stat->location.end < pos)
                continue;
            if (stat->location.begin > pos)
                break;

            stat->visit(this);
        }

        return false;
    }
};

Luau::AstNode* findNodeOrTypeAtPosition(const Luau::SourceModule& source, Luau::Position pos);
Luau::ExprOrLocal findExprOrLocalAtPositionClosed(const Luau::SourceModule& source, Luau::Position pos);
std::vector<Luau::Location> findSymbolReferences(const Luau::SourceModule& source, Luau::Symbol symbol);

std::optional<Luau::Location> getLocation(Luau::TypeId type);

std::optional<Luau::Location> lookupTypeLocation(const Luau::Scope& deepScope, const Luau::Name& name);
std::optional<Luau::Property> lookupProp(const Luau::TypeId& parentType, const Luau::Name& name);

// Converts a UTF-8 position to a UTF-16 position, using the provided text document if available
// NOTE: if the text document doesn't exist, we perform no conversion, so the positioning may be
// incorrect
lsp::Position toUTF16(const TextDocument* textDocument, const Luau::Position& position);

lsp::Diagnostic createTypeErrorDiagnostic(const Luau::TypeError& error, Luau::FileResolver* fileResolver, const TextDocument* textDocument = nullptr);
lsp::Diagnostic createLintDiagnostic(const Luau::LintWarning& lint, const TextDocument* textDocument = nullptr);
lsp::Diagnostic createParseErrorDiagnostic(const Luau::ParseError& error, const TextDocument* textDocument = nullptr);