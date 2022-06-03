#include <climits>
#include "Luau/BuiltinDefinitions.h"
#include "Luau/ToString.h"
#include "Luau/Transpiler.h"
#include "LSP/LuauExt.hpp"
#include "LSP/Utils.hpp"

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

Luau::TypeId makeLazyInstanceType(
    Luau::TypeArena& arena, const Luau::ScopePtr& globalScope, const SourceNodePtr& node, std::optional<Luau::TypeId> parent)
{
    Luau::LazyTypeVar ltv;
    ltv.thunk = [&arena, globalScope, node, parent]()
    {
        // TODO: we should cache created instance types and reuse them where possible

        // Handle if the node is no longer valid
        if (!node)
            return Luau::getSingletonTypes().anyType;

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
            else if (auto parentNode = node->parent.lock())
            {
                ctv->props["Parent"] = Luau::makeProperty(makeLazyInstanceType(arena, globalScope, parentNode, std::nullopt));
            }

            // Add the children
            for (const auto& child : node->children)
            {
                ctv->props[child->name] = Luau::makeProperty(makeLazyInstanceType(arena, globalScope, child, typeId));
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
std::optional<Luau::ExprResult<Luau::TypePackId>> magicFunctionInstanceClone(
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
        // If we are calling this as a method ':', we should implicitly hide self, and recompute the functionString
        opts.hideFunctionSelfArgument = indexName->op == ':';
        functionString = Luau::toStringNamedFunction("", *ftv, opts);
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

std::optional<Luau::Property> lookupProp(const Luau::TypeId& parentType, const Luau::Name& name)
{
    if (auto ctv = Luau::get<Luau::ClassTypeVar>(parentType))
    {
        if (auto prop = Luau::lookupClassProp(ctv, name))
            return *prop;
    }
    else if (auto tbl = Luau::get<Luau::TableTypeVar>(parentType))
    {
        if (tbl->props.find(name) != tbl->props.end())
        {
            return tbl->props.at(name);
        }
    }
    else if (auto mt = Luau::get<Luau::MetatableTypeVar>(parentType))
    {
        if (auto tbl = Luau::get<Luau::TableTypeVar>(mt->table))
        {
            if (tbl->props.find(name) != tbl->props.end())
            {
                return tbl->props.at(name);
            }
        }

        // TODO: we should respect metatable __index
    }
    // else if (auto i = get<Luau::IntersectionTypeVar>(parentType))
    // {
    //     for (Luau::TypeId ty : i->parts)
    //     {
    //         // TODO: find the corresponding ty
    //     }
    // }
    // else if (auto u = get<Luau::UnionTypeVar>(parentType))
    // {
    //     // Find the corresponding ty
    // }
    return std::nullopt;
}

Luau::Position convertPosition(const lsp::Position& position)
{
    LUAU_ASSERT(position.line <= UINT_MAX);
    LUAU_ASSERT(position.character <= UINT_MAX);
    return Luau::Position{static_cast<unsigned int>(position.line), static_cast<unsigned int>(position.character)};
}

lsp::Position convertPosition(const Luau::Position& position)
{
    return lsp::Position{static_cast<size_t>(position.line), static_cast<size_t>(position.column)};
}

lsp::Diagnostic createTypeErrorDiagnostic(const Luau::TypeError& error)
{
    std::string message;
    if (const Luau::SyntaxError* syntaxError = Luau::get_if<Luau::SyntaxError>(&error.data))
        message = "SyntaxError: " + syntaxError->message;
    else
        message = "TypeError: " + Luau::toString(error);

    lsp::Diagnostic diagnostic;
    diagnostic.source = "Luau";
    diagnostic.code = error.code();
    diagnostic.message = message;
    diagnostic.severity = lsp::DiagnosticSeverity::Error;
    diagnostic.range = {convertPosition(error.location.begin), convertPosition(error.location.end)};
    return diagnostic;
}

lsp::Diagnostic createLintDiagnostic(const Luau::LintWarning& lint)
{
    lsp::Diagnostic diagnostic;
    diagnostic.source = "Luau";
    diagnostic.code = lint.code;
    diagnostic.message = std::string(Luau::LintWarning::getName(lint.code)) + ": " + lint.text;
    diagnostic.severity = lsp::DiagnosticSeverity::Warning; // Configuration can convert this to an error
    diagnostic.range = {convertPosition(lint.location.begin), convertPosition(lint.location.end)};
    return diagnostic;
}

struct FindSymbolReferences : public Luau::AstVisitor
{
    const Luau::Symbol symbol;
    std::vector<Luau::Location> result;

    explicit FindSymbolReferences(Luau::Symbol symbol)
        : symbol(symbol)
    {
    }

    bool visit(Luau::AstStatBlock* block) override
    {
        for (Luau::AstStat* stat : block->body)
        {
            stat->visit(this);
        }

        return false;
    }

    bool visitLocal(Luau::AstLocal* local)
    {
        if (Luau::Symbol(local) == symbol)
        {
            return true;
        }
        return false;
    }

    bool visit(Luau::AstStatLocalFunction* function) override
    {
        if (visitLocal(function->name))
        {
            result.push_back(function->name->location);
        }
        return true;
    }

    bool visit(Luau::AstStatLocal* al) override
    {
        for (size_t i = 0; i < al->vars.size; ++i)
        {
            if (visitLocal(al->vars.data[i]))
            {
                result.push_back(al->vars.data[i]->location);
            }
        }
        return true;
    }

    virtual bool visit(Luau::AstExprLocal* local) override
    {
        if (visitLocal(local->local))
        {
            result.push_back(local->location);
        }
        return true;
    }

    virtual bool visit(Luau::AstExprFunction* fn) override
    {
        for (size_t i = 0; i < fn->args.size; ++i)
        {
            if (visitLocal(fn->args.data[i]))
            {
                result.push_back(fn->args.data[i]->location);
            }
        }
        return true;
    }

    virtual bool visit(Luau::AstStatFor* forStat) override
    {
        if (visitLocal(forStat->var))
        {
            result.push_back(forStat->var->location);
        }
        return true;
    }

    virtual bool visit(Luau::AstStatForIn* forIn) override
    {
        for (auto var : forIn->vars)
        {
            if (visitLocal(var))
            {
                result.push_back(var->location);
            }
        }
        return true;
    }
};

std::vector<Luau::Location> findSymbolReferences(const Luau::SourceModule& source, Luau::Symbol symbol)
{
    FindSymbolReferences finder(symbol);
    source.root->visit(&finder);
    return std::move(finder.result);
}