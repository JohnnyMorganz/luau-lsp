#pragma once
#include <optional>
#include "Luau/Frontend.h"
#include "Luau/Scope.h"
#include "LSP/Protocol.hpp"
#include "LSP/WorkspaceFileResolver.hpp"

namespace types
{
std::optional<Luau::TypeId> getTypeIdForClass(const Luau::ScopePtr& globalScope, std::optional<std::string> className);
std::optional<std::string> getTypeName(Luau::TypeId typeId);

Luau::TypeId makeLazyInstanceType(Luau::TypeArena& arena, const Luau::ScopePtr& globalScope, const SourceNodePtr& node,
    std::optional<Luau::TypeId> parent, std::optional<Luau::TypeId> baseClass = std::nullopt);

// Magic function for `Instance:IsA("ClassName")` predicate
std::optional<Luau::ExprResult<Luau::TypePackId>> magicFunctionInstanceIsA(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::ExprResult<Luau::TypePackId> exprResult);

// Magic function for `instance:Clone()`, so that we return the exact subclass that `instance` is, rather than just a generic Instance
std::optional<Luau::ExprResult<Luau::TypePackId>> magicFunctionInstanceClone(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::ExprResult<Luau::TypePackId> exprResult);

// Magic function for `Instance:FindFirstChildWhichIsA("ClassName")` and friends
std::optional<Luau::ExprResult<Luau::TypePackId>> magicFunctionFindFirstXWhichIsA(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::ExprResult<Luau::TypePackId> exprResult);

using NameOrExpr = std::variant<std::string, Luau::AstExpr*>;

// Converts a FTV and function call to a nice string
// In the format "function NAME(args): ret"
std::string toStringNamedFunction(Luau::ModulePtr module, const Luau::FunctionTypeVar* ftv, const NameOrExpr nameOrFuncExpr);

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
std::vector<Luau::Location> findSymbolReferences(const Luau::SourceModule& source, Luau::Symbol symbol);

std::optional<Luau::Location> lookupTypeLocation(const Luau::Scope& deepScope, const Luau::Name& name);
std::optional<Luau::Property> lookupProp(const Luau::TypeId& parentType, const Luau::Name& name);

Luau::Position convertPosition(const lsp::Position& position);
lsp::Position convertPosition(const Luau::Position& position);

lsp::Diagnostic createTypeErrorDiagnostic(const Luau::TypeError& error);
lsp::Diagnostic createLintDiagnostic(const Luau::LintWarning& lint);