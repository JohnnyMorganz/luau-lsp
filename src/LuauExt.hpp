#pragma once
#include <optional>
#include "WorkspaceFileResolver.hpp"

namespace types
{
std::optional<Luau::TypeId> getTypeIdForClass(const Luau::ScopePtr& globalScope, std::optional<std::string> className)
{
    std::optional<Luau::TypeFun> baseType;
    if (className.has_value())
    {
        baseType = globalScope->lookupType(className.value());
    }
    if (!baseType.has_value())
    {
        baseType = globalScope->lookupType("Instance");
    }

    if (baseType.has_value())
    {
        return baseType->type;
    }
    else
    {
        // If we reach this stage, we couldn't find the class name nor the "Instance" type
        // This most likely means a valid definitions file was not provided
        return std::nullopt;
    }
}

Luau::TypeId makeLazyInstanceType(Luau::TypeArena& arena, const Luau::ScopePtr& globalScope, const SourceNodePtr& node,
    std::optional<Luau::TypeId> parent, const WorkspaceFileResolver& fileResolver)
{
    Luau::LazyTypeVar ltv;
    ltv.thunk = [&arena, globalScope, node, parent, fileResolver]()
    {
        // TODO: we should cache created instance types and reuse them where possible

        // Look up the base class instance
        auto baseTypeId = getTypeIdForClass(globalScope, node->className);
        if (!baseTypeId)
        {
            return Luau::getSingletonTypes().anyType;
        }

        // Create the ClassTypeVar representing the instance
        Luau::ClassTypeVar ctv{node->name, {}, baseTypeId, std::nullopt, {}, {}, "@roblox"};
        auto typeId = arena.addType(std::move(ctv));

        // Attach Parent and Children info
        // Get the mutable version of the type var
        if (Luau::ClassTypeVar* ctv = Luau::getMutable<Luau::ClassTypeVar>(typeId))
        {

            // Add the parent
            if (parent.has_value())
            {
                ctv->props["Parent"] = Luau::makeProperty(parent.value());
            }
            else
            {
                // Search for the parent type
                if (auto parentPath = getParentPath(node->virtualPath))
                {
                    if (auto parentNode = fileResolver.getSourceNodeFromVirtualPath(parentPath.value()))
                    {
                        ctv->props["Parent"] =
                            Luau::makeProperty(makeLazyInstanceType(arena, globalScope, parentNode.value(), std::nullopt, fileResolver));
                    }
                }
            }

            // Add the children
            for (const auto& child : node->children)
            {
                ctv->props[child->name] = Luau::makeProperty(makeLazyInstanceType(arena, globalScope, child, typeId, fileResolver));
            }
        }
        return typeId;
    };

    return arena.addType(std::move(ltv));
}

// Magic function for `Instance:IsA("ClassName")` predicate
std::optional<Luau::ExprResult<Luau::TypePackId>> magicFunctionInstanceIsA(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::ExprResult<Luau::TypePackId> exprResult)
{
    if (expr.args.size != 1)
        return std::nullopt;

    auto index = expr.func->as<Luau::AstExprIndexName>();
    auto str = expr.args.data[0]->as<Luau::AstExprConstantString>();
    if (!index || !str)
        return std::nullopt;

    std::optional<Luau::LValue> lvalue = tryGetLValue(*index->expr);
    std::optional<Luau::TypeFun> tfun = scope->lookupType(std::string(str->value.data, str->value.size));
    if (!lvalue || !tfun)
        return std::nullopt;

    Luau::TypePackId booleanPack = typeChecker.globalTypes.addTypePack({typeChecker.booleanType});
    return Luau::ExprResult<Luau::TypePackId>{booleanPack, {Luau::IsAPredicate{std::move(*lvalue), expr.location, tfun->type}}};
}

// Magic function for `instance:Clone()`, so that we return the exact subclass that `instance` is, rather than just a generic Instance
static std::optional<Luau::ExprResult<Luau::TypePackId>> magicFunctionInstanceClone(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::ExprResult<Luau::TypePackId> exprResult)
{
    auto index = expr.func->as<Luau::AstExprIndexName>();
    if (!index)
        return std::nullopt;

    Luau::TypeArena& arena = typeChecker.currentModule->internalTypes;
    Luau::TypeId instanceType = typeChecker.checkLValueBinding(scope, *index->expr);
    return Luau::ExprResult<Luau::TypePackId>{arena.addTypePack({instanceType})};
}

// Magic function for `Instance:FindFirstChildWhichIsA("ClassName")` and friends
std::optional<Luau::ExprResult<Luau::TypePackId>> magicFunctionFindFirstXWhichIsA(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::ExprResult<Luau::TypePackId> exprResult)
{
    if (expr.args.size < 1)
        return std::nullopt;

    auto str = expr.args.data[0]->as<Luau::AstExprConstantString>();
    if (!str)
        return std::nullopt;

    std::optional<Luau::TypeFun> tfun = scope->lookupType(std::string(str->value.data, str->value.size));
    if (!tfun)
        return std::nullopt;

    Luau::TypeId nillableClass = Luau::makeOption(typeChecker, typeChecker.globalTypes, tfun->type);
    return Luau::ExprResult<Luau::TypePackId>{typeChecker.globalTypes.addTypePack({nillableClass})};
}

using NameOrExpr = std::variant<std::string, Luau::AstExpr*>;

// Converts a FTV and function call to a nice string
// In the format "function NAME(args): ret"
std::string toStringNamedFunction(Luau::ModulePtr module, const Luau::FunctionTypeVar* ftv, const NameOrExpr nameOrFuncExpr)
{
    Luau::ToStringOptions opts;
    opts.functionTypeArguments = true;
    opts.hideNamedFunctionTypeParameters = false;
    auto functionString = Luau::toStringNamedFunction("", *ftv, opts);

    // HACK: remove all instances of "_: " from the function string
    // They don't look great, maybe we should upstream this as an option?
    size_t index;
    while ((index = functionString.find("_: ")) != std::string::npos)
    {
        functionString.replace(index, 3, "");
    }

    // If a name has already been provided, just use that
    if (auto name = std::get_if<std::string>(&nameOrFuncExpr))
    {
        return "function " + *name + functionString;
    }
    auto funcExprPtr = std::get_if<Luau::AstExpr*>(&nameOrFuncExpr);

    // TODO: error here?
    if (!funcExprPtr)
        return "function" + functionString;
    auto funcExpr = *funcExprPtr;

    // See if its just in the form `func(args)`
    if (auto local = funcExpr->as<Luau::AstExprLocal>())
    {
        return "function " + std::string(local->local->name.value) + functionString;
    }
    else if (auto global = funcExpr->as<Luau::AstExprGlobal>())
    {
        return "function " + std::string(global->name.value) + functionString;
    }
    else if (funcExpr->as<Luau::AstExprGroup>() || funcExpr->as<Luau::AstExprFunction>())
    {
        // In the form (expr)(args), which implies thats its probably a IIFE
        return "function" + functionString;
    }

    // See if the name belongs to a ClassTypeVar
    Luau::TypeId* parentIt = nullptr;
    std::string methodName;
    std::string baseName;

    if (auto indexName = funcExpr->as<Luau::AstExprIndexName>())
    {
        parentIt = module->astTypes.find(indexName->expr);
        methodName = std::string(1, indexName->op) + indexName->index.value;
        // If we are calling this as a method ':', we should implicitly hide self
        opts.hideFunctionSelfArgument = indexName->op == ':';
        // We can try and give a temporary base name from what we can infer by the index, and then attempt to improve it with proper information
        baseName = Luau::toString(indexName->expr);
        trim(baseName); // Trim it, because toString is probably not meant to be used in this context (it has whitespace)
    }
    else if (auto indexExpr = funcExpr->as<Luau::AstExprIndexExpr>())
    {
        parentIt = module->astTypes.find(indexExpr->expr);
        methodName = Luau::toString(indexExpr->index);
        // We can try and give a temporary base name from what we can infer by the index, and then attempt to improve it with proper information
        baseName = Luau::toString(indexName->expr);
        trim(baseName); // Trim it, because toString is probably not meant to be used in this context (it has whitespace)
    }

    if (parentIt)
    {
        Luau::TypeId parentType = Luau::follow(*parentIt);
        if (auto typeName = Luau::getName(parentType))
        {
            baseName = *typeName;
        }
        else if (auto parentClass = Luau::get<Luau::ClassTypeVar>(parentType))
        {
            baseName = parentClass->name;
        }
        // if (auto parentUnion = Luau::get<UnionTypeVar>(parentType))
        // {
        //     return returnFirstNonnullOptionOfType<ClassTypeVar>(parentUnion);
        // }
    }
    else
    {
        // TODO: anymore we can do?
        baseName = "_";
    }

    return "function " + baseName + methodName + functionString;
}

// Duplicated from Luau/TypeInfer.h, since its static
std::optional<Luau::AstExpr*> matchRequire(const Luau::AstExprCall& call)
{
    const char* require = "require";

    if (call.args.size != 1)
        return std::nullopt;

    const Luau::AstExprGlobal* funcAsGlobal = call.func->as<Luau::AstExprGlobal>();
    if (!funcAsGlobal || funcAsGlobal->name != require)
        return std::nullopt;

    if (call.args.size != 1)
        return std::nullopt;

    return call.args.data[0];
}


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

    virtual bool visit(class Luau::AstType* node)
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

Luau::AstNode* findNodeOrTypeAtPosition(const Luau::SourceModule& source, Luau::Position pos)
{
    const Luau::Position end = source.root->location.end;
    if (pos < source.root->location.begin)
        return source.root;

    if (pos > end)
        pos = end;

    FindNodeType findNode{pos, end};
    findNode.visit(source.root);
    return findNode.best;
}

std::optional<Luau::Location> lookupTypeLocation(const Luau::Scope& deepScope, const Luau::Name& name)
{
    const Luau::Scope* scope = &deepScope;
    while (true)
    {
        auto it = scope->typeAliasLocations.find(name);
        if (it != scope->typeAliasLocations.end())
            return it->second;

        if (scope->parent)
            scope = scope->parent.get();
        else
            return std::nullopt;
    }
}