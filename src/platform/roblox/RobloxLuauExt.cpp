#include "Platform/RobloxPlatform.hpp"

#include "Luau/BuiltinDefinitions.h"
#include "Luau/ConstraintSolver.h"
#include "Luau/TypeInfer.h"

// Magic function for `Instance:IsA("ClassName")` predicate
struct MagicInstanceIsA final : Luau::MagicFunction
{
    std::optional<Luau::WithPredicate<Luau::TypePackId>> handleOldSolver(struct Luau::TypeChecker&, const std::shared_ptr<struct Luau::Scope>&,
        const class Luau::AstExprCall&, Luau::WithPredicate<Luau::TypePackId>) override;
    bool infer(const Luau::MagicFunctionCallContext& context) override;
    void refine(const Luau::MagicRefinementContext& context) override;
};

std::optional<Luau::WithPredicate<Luau::TypePackId>> MagicInstanceIsA::handleOldSolver(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId>)
{
    if (expr.args.size != 1)
        return std::nullopt;

    auto index = expr.func->as<Luau::AstExprIndexName>();
    auto str = expr.args.data[0]->as<Luau::AstExprConstantString>();
    if (!index || !str)
        return std::nullopt;

    std::optional<Luau::LValue> lvalue = tryGetLValue(*index->expr);
    if (!lvalue)
        return std::nullopt;

    std::string className(str->value.data, str->value.size);
    std::optional<Luau::TypeFun> tfun = typeChecker.globalScope->lookupType(className);
    if (!tfun || !tfun->typeParams.empty() || !tfun->typePackParams.empty())
    {
        typeChecker.reportError(Luau::TypeError{expr.args.data[0]->location, Luau::UnknownSymbol{className, Luau::UnknownSymbol::Type}});
        return std::nullopt;
    }

    auto type = Luau::follow(tfun->type);

    Luau::TypeArena& arena = typeChecker.currentModule->internalTypes;
    Luau::TypePackId booleanPack = arena.addTypePack({typeChecker.booleanType});
    return Luau::WithPredicate<Luau::TypePackId>{booleanPack, {Luau::IsAPredicate{std::move(*lvalue), expr.location, type}}};
}

bool MagicInstanceIsA::infer(const Luau::MagicFunctionCallContext& context)
{
    if (context.callSite->args.size != 1)
        return false;

    auto index = context.callSite->func->as<Luau::AstExprIndexName>();
    auto str = context.callSite->args.data[0]->as<Luau::AstExprConstantString>();
    if (!index || !str)
        return false;

    std::string className(str->value.data, str->value.size);
    std::optional<Luau::TypeFun> tfun = context.constraint->scope->lookupType(className);
    if (!tfun)
        context.solver->reportError(
            Luau::TypeError{context.callSite->args.data[0]->location, Luau::UnknownSymbol{className, Luau::UnknownSymbol::Type}});

    return false;
}

void MagicInstanceIsA::refine(const Luau::MagicRefinementContext& context)
{
    if (context.callSite->args.size != 1 || context.discriminantTypes.empty())
        return;

    auto index = context.callSite->func->as<Luau::AstExprIndexName>();
    auto str = context.callSite->args.data[0]->as<Luau::AstExprConstantString>();
    if (!index || !str)
        return;

    std::optional<Luau::TypeId> discriminantTy = context.discriminantTypes[0];
    if (!discriminantTy)
        return;

    std::string className(str->value.data, str->value.size);
    std::optional<Luau::TypeFun> tfun = context.scope->lookupType(className);
    if (!tfun)
        return;

    LUAU_ASSERT(Luau::get<Luau::BlockedType>(*discriminantTy));
    asMutable(*discriminantTy)->ty.emplace<Luau::BoundType>(Luau::follow(tfun->type));
}

// Magic function for `instance:Clone()`, so that we return the exact subclass that `instance` is, rather than just a generic Instance
struct MagicInstanceClone final : Luau::MagicFunction
{
    std::optional<Luau::WithPredicate<Luau::TypePackId>> handleOldSolver(Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope,
        const Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId> withPredicate) override;
    bool infer(const Luau::MagicFunctionCallContext& context) override;
};

std::optional<Luau::WithPredicate<Luau::TypePackId>> MagicInstanceClone::handleOldSolver(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId>)
{
    auto index = expr.func->as<Luau::AstExprIndexName>();
    if (!index)
        return std::nullopt;

    Luau::TypeArena& arena = typeChecker.currentModule->internalTypes;
    auto instanceType = typeChecker.checkExpr(scope, *index->expr);
    return Luau::WithPredicate<Luau::TypePackId>{arena.addTypePack({instanceType.type})};
}

bool MagicInstanceClone::infer(const Luau::MagicFunctionCallContext& context)
{
    auto index = context.callSite->func->as<Luau::AstExprIndexName>();
    if (!index)
        return false;

    // The cloned type is the self type, i.e. the first argument
    auto selfTy = Luau::first(context.arguments);
    if (!selfTy)
        return false;

    asMutable(context.result)->ty.emplace<Luau::BoundTypePack>(context.solver->arena->addTypePack({*selfTy}));
    return true;
}

struct MagicInstanceFromExisting final : Luau::MagicFunction
{
    std::optional<Luau::WithPredicate<Luau::TypePackId>> handleOldSolver(struct Luau::TypeChecker& typeChecker,
        const std::shared_ptr<struct Luau::Scope>& scope, const class Luau::AstExprCall& expr,
        Luau::WithPredicate<Luau::TypePackId> withPredicate) override;
    bool infer(const Luau::MagicFunctionCallContext& context) override;
};

std::optional<Luau::WithPredicate<Luau::TypePackId>> MagicInstanceFromExisting::handleOldSolver(struct Luau::TypeChecker& typeChecker,
    const std::shared_ptr<struct Luau::Scope>& scope, const class Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId> withPredicate)
{
    if (expr.args.size < 1)
        return std::nullopt;

    Luau::TypeArena& arena = typeChecker.currentModule->internalTypes;
    auto instanceType = typeChecker.checkExpr(scope, *expr.args.data[0]);
    return Luau::WithPredicate<Luau::TypePackId>{arena.addTypePack({instanceType.type})};
}

bool MagicInstanceFromExisting::infer(const Luau::MagicFunctionCallContext& context)
{
    if (context.callSite->args.size < 1)
        return false;

    // The cloned type is the first argument
    auto clonedTy = Luau::first(context.arguments);
    if (!clonedTy)
        return false;

    asMutable(context.result)->ty.emplace<Luau::BoundTypePack>(context.solver->arena->addTypePack({*clonedTy}));
    return true;
}

// Magic function for `Instance:FindFirstChildWhichIsA("ClassName")` and friends
struct MagicInstanceFindFirstXWhichIsA final : Luau::MagicFunction
{
    std::optional<Luau::WithPredicate<Luau::TypePackId>> handleOldSolver(struct Luau::TypeChecker& typeChecker,
        const std::shared_ptr<struct Luau::Scope>& scope, const class Luau::AstExprCall& expr,
        Luau::WithPredicate<Luau::TypePackId> withPredicate) override;
    bool infer(const Luau::MagicFunctionCallContext& context) override;
};

std::optional<Luau::WithPredicate<Luau::TypePackId>> MagicInstanceFindFirstXWhichIsA::handleOldSolver(struct Luau::TypeChecker& typeChecker,
    const std::shared_ptr<struct Luau::Scope>& scope, const class Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId> withPredicate)
{
    if (expr.args.size < 1)
        return std::nullopt;

    auto str = expr.args.data[0]->as<Luau::AstExprConstantString>();
    if (!str)
        return std::nullopt;

    std::optional<Luau::TypeFun> tfun = scope->lookupType(std::string(str->value.data, str->value.size));
    if (!tfun || !tfun->typeParams.empty() || !tfun->typePackParams.empty())
        return std::nullopt;

    auto type = Luau::follow(tfun->type);

    Luau::TypeArena& arena = typeChecker.currentModule->internalTypes;
    Luau::TypeId nillableClass = Luau::makeOption(typeChecker.builtinTypes, arena, type);
    return Luau::WithPredicate<Luau::TypePackId>{arena.addTypePack({nillableClass})};
}

bool MagicInstanceFindFirstXWhichIsA::infer(const Luau::MagicFunctionCallContext& context)
{
    if (context.callSite->args.size < 1)
        return false;

    auto str = context.callSite->args.data[0]->as<Luau::AstExprConstantString>();
    if (!str)
        return false;

    std::optional<Luau::TypeFun> tfun = context.constraint->scope->lookupType(std::string(str->value.data, str->value.size));
    if (!tfun || !tfun->typeParams.empty() || !tfun->typePackParams.empty())
        return false;

    auto type = Luau::follow(tfun->type);

    Luau::TypeId nillableClass = Luau::makeOption(context.solver->builtinTypes, *context.solver->arena, type);
    asMutable(context.result)->ty.emplace<Luau::BoundTypePack>(context.solver->arena->addTypePack({nillableClass}));
    return true;
}

// Magic function for `EnumItem:IsA("EnumType")` predicate
struct MagicEnumItemIsA final : Luau::MagicFunction
{
    std::optional<Luau::WithPredicate<Luau::TypePackId>> handleOldSolver(struct Luau::TypeChecker& typeChecker,
        const std::shared_ptr<struct Luau::Scope>& scope, const class Luau::AstExprCall& expr,
        Luau::WithPredicate<Luau::TypePackId> withPredicate) override;
    bool infer(const Luau::MagicFunctionCallContext& context) override;
    void refine(const Luau::MagicRefinementContext& context) override;
};

std::optional<Luau::WithPredicate<Luau::TypePackId>> MagicEnumItemIsA::handleOldSolver(struct Luau::TypeChecker& typeChecker,
    const std::shared_ptr<struct Luau::Scope>& scope, const class Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId> withPredicate)
{
    if (expr.args.size != 1)
        return std::nullopt;

    auto index = expr.func->as<Luau::AstExprIndexName>();
    auto str = expr.args.data[0]->as<Luau::AstExprConstantString>();
    if (!index || !str)
        return std::nullopt;

    std::optional<Luau::LValue> lvalue = tryGetLValue(*index->expr);
    if (!lvalue)
        return std::nullopt;

    std::string enumItem(str->value.data, str->value.size);
    std::optional<Luau::TypeFun> tfun = scope->lookupImportedType("Enum", enumItem);
    if (!tfun || !tfun->typeParams.empty() || !tfun->typePackParams.empty())
    {
        typeChecker.reportError(Luau::TypeError{expr.args.data[0]->location, Luau::UnknownSymbol{enumItem, Luau::UnknownSymbol::Type}});
        return std::nullopt;
    }

    auto type = Luau::follow(tfun->type);

    Luau::TypeArena& arena = typeChecker.currentModule->internalTypes;
    Luau::TypePackId booleanPack = arena.addTypePack({typeChecker.booleanType});
    return Luau::WithPredicate<Luau::TypePackId>{booleanPack, {Luau::IsAPredicate{std::move(*lvalue), expr.location, type}}};
}

bool MagicEnumItemIsA::infer(const Luau::MagicFunctionCallContext& context)
{
    if (context.callSite->args.size != 1)
        return false;

    auto index = context.callSite->func->as<Luau::AstExprIndexName>();
    auto str = context.callSite->args.data[0]->as<Luau::AstExprConstantString>();
    if (!index || !str)
        return false;

    std::string enumItem(str->value.data, str->value.size);
    std::optional<Luau::TypeFun> tfun = context.constraint->scope->lookupImportedType("Enum", enumItem);
    if (!tfun)
        context.solver->reportError(
            Luau::TypeError{context.callSite->args.data[0]->location, Luau::UnknownSymbol{enumItem, Luau::UnknownSymbol::Type}});

    return false;
}

void MagicEnumItemIsA::refine(const Luau::MagicRefinementContext& context)
{
    if (context.callSite->args.size != 1 || context.discriminantTypes.empty())
        return;

    auto index = context.callSite->func->as<Luau::AstExprIndexName>();
    auto str = context.callSite->args.data[0]->as<Luau::AstExprConstantString>();
    if (!index || !str)
        return;

    std::optional<Luau::TypeId> discriminantTy = context.discriminantTypes[0];
    if (!discriminantTy)
        return;

    std::string enumItem(str->value.data, str->value.size);
    std::optional<Luau::TypeFun> tfun = context.scope->lookupImportedType("Enum", enumItem);
    if (!tfun)
        return;

    LUAU_ASSERT(Luau::get<Luau::BlockedType>(*discriminantTy));
    asMutable(*discriminantTy)->ty.emplace<Luau::BoundType>(Luau::follow(tfun->type));
}

// Magic function for `instance:GetPropertyChangedSignal()`, so that we can perform type checking on the provided property
struct MagicGetPropertyChangedSignal final : Luau::MagicFunction
{
    std::optional<Luau::WithPredicate<Luau::TypePackId>> handleOldSolver(struct Luau::TypeChecker& typeChecker,
        const std::shared_ptr<struct Luau::Scope>& scope, const class Luau::AstExprCall& expr,
        Luau::WithPredicate<Luau::TypePackId> withPredicate) override;
    bool infer(const Luau::MagicFunctionCallContext& context) override;
};

std::optional<Luau::WithPredicate<Luau::TypePackId>> MagicGetPropertyChangedSignal::handleOldSolver(struct Luau::TypeChecker& typeChecker,
    const std::shared_ptr<struct Luau::Scope>& scope, const class Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId> withPredicate)
{
    if (expr.args.size != 1)
        return std::nullopt;

    auto index = expr.func->as<Luau::AstExprIndexName>();
    auto str = expr.args.data[0]->as<Luau::AstExprConstantString>();
    if (!index || !str)
        return std::nullopt;


    auto instanceType = typeChecker.checkExpr(scope, *index->expr);
    auto ctv = Luau::get<Luau::ExternType>(Luau::follow(instanceType.type));
    if (!ctv)
        return std::nullopt;

    std::string property(str->value.data, str->value.size);
    if (!Luau::lookupExternTypeProp(ctv, property))
    {
        typeChecker.reportError(Luau::TypeError{expr.args.data[0]->location, Luau::UnknownProperty{instanceType.type, property}});
        return std::nullopt;
    }

    return std::nullopt;
}

bool MagicGetPropertyChangedSignal::infer(const Luau::MagicFunctionCallContext& context)
{
    if (context.callSite->args.size != 1)
        return false;

    auto index = context.callSite->func->as<Luau::AstExprIndexName>();
    auto str = context.callSite->args.data[0]->as<Luau::AstExprConstantString>();
    if (!index || !str)
        return false;

    // The cloned type is the self type, i.e. the first argument
    auto selfTy = Luau::first(context.arguments);
    if (!selfTy)
        return false;

    auto ctv = Luau::get<Luau::ExternType>(Luau::follow(selfTy));
    if (!ctv)
        return false;

    std::string property(str->value.data, str->value.size);
    if (!Luau::lookupExternTypeProp(ctv, property))
    {
        context.solver->reportError(Luau::TypeError{context.callSite->args.data[0]->location, Luau::UnknownProperty{*selfTy, property}});
    }

    return false;
}

// Since in Roblox land, debug is extended to introduce more methods, but the api-docs
// mark the package name as `@luau` instead of `@roblox`
static void fixDebugDocumentationSymbol(Luau::TypeId ty, const std::string& libraryName)
{
    auto mutableTy = Luau::asMutable(ty);
    auto newDocumentationSymbol = mutableTy->documentationSymbol.value();
    replace(newDocumentationSymbol, "@roblox", "@luau");
    mutableTy->documentationSymbol = newDocumentationSymbol;

    if (auto* ttv = Luau::getMutable<Luau::TableType>(ty))
    {
        ttv->name = "typeof(" + libraryName + ")";
        for (auto& [name, prop] : ttv->props)
        {
            newDocumentationSymbol = prop.documentationSymbol.value();
            replace(newDocumentationSymbol, "@roblox", "@luau");
            prop.documentationSymbol = newDocumentationSymbol;
        }
    }
}

struct MagicTypeLookup final : Luau::MagicFunction
{
    std::vector<std::string> lookupList;
    std::string errorMessagePrefix;

    MagicTypeLookup(std::vector<std::string> lookupList, std::string errorMessagePrefix)
        : lookupList(std::move(lookupList))
        , errorMessagePrefix(std::move(errorMessagePrefix))
    {
    }

    std::optional<Luau::WithPredicate<Luau::TypePackId>> handleOldSolver(struct Luau::TypeChecker& typeChecker,
        const std::shared_ptr<struct Luau::Scope>& scope, const class Luau::AstExprCall& expr,
        Luau::WithPredicate<Luau::TypePackId> withPredicate) override;
    bool infer(const Luau::MagicFunctionCallContext&) override;
};

std::optional<Luau::WithPredicate<Luau::TypePackId>> MagicTypeLookup::handleOldSolver(struct Luau::TypeChecker& typeChecker,
    const std::shared_ptr<struct Luau::Scope>&, const class Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId>)
{
    if (expr.args.size < 1)
        return std::nullopt;

    if (auto str = expr.args.data[0]->as<Luau::AstExprConstantString>())
    {
        auto className = std::string(str->value.data, str->value.size);
        if (contains(lookupList, className))
        {
            std::optional<Luau::TypeFun> tfun = typeChecker.globalScope->lookupType(className);
            if (!tfun || !tfun->typeParams.empty() || !tfun->typePackParams.empty())
            {
                typeChecker.reportError(Luau::TypeError{expr.args.data[0]->location, Luau::UnknownSymbol{className, Luau::UnknownSymbol::Type}});
                return std::nullopt;
            }

            auto type = Luau::follow(tfun->type);

            Luau::TypeArena& arena = typeChecker.currentModule->internalTypes;
            Luau::TypePackId classTypePack = arena.addTypePack({type});
            return Luau::WithPredicate<Luau::TypePackId>{classTypePack};
        }
        else
        {
            typeChecker.reportError(Luau::TypeError{expr.args.data[0]->location, Luau::GenericError{errorMessagePrefix + " '" + className + "'"}});
        }
    }

    return std::nullopt;
};

bool MagicTypeLookup::infer(const Luau::MagicFunctionCallContext& context)
{
    if (context.callSite->args.size < 1)
        return false;

    if (auto str = context.callSite->args.data[0]->as<Luau::AstExprConstantString>())
    {
        auto className = std::string(str->value.data, str->value.size);
        if (contains(lookupList, className))
        {
            // TODO: only check the global scope?
            std::optional<Luau::TypeFun> tfun = context.constraint->scope->lookupType(className);
            if (!tfun || !tfun->typeParams.empty() || !tfun->typePackParams.empty())
            {
                context.solver->reportError(
                    Luau::TypeError{context.callSite->args.data[0]->location, Luau::UnknownSymbol{className, Luau::UnknownSymbol::Type}});
                return false;
            }

            auto type = Luau::follow(tfun->type);
            Luau::TypePackId classTypePack = context.solver->arena->addTypePack({type});
            asMutable(context.result)->ty.emplace<Luau::BoundTypePack>(classTypePack);
            return true;
        }
        else
        {
            context.solver->reportError(
                Luau::TypeError{context.callSite->args.data[0]->location, Luau::GenericError{errorMessagePrefix + " '" + className + "'"}});
        }
    }

    return false;
};

static void attachMagicFunctionSafe(Luau::TableType::Props& props, const char* property, std::shared_ptr<Luau::MagicFunction> magic)
{
    if (const auto prop = props.find(property); prop != props.end())
        Luau::attachMagicFunction(prop->second.type(), magic);
}

static void attachTagSafe(Luau::TableType::Props& props, const char* property, const char* tagName)
{
    if (const auto prop = props.find(property); prop != props.end())
        Luau::attachTag(prop->second.type(), tagName);
}

void RobloxPlatform::mutateRegisteredDefinitions(Luau::GlobalTypes& globals, std::optional<nlohmann::json> metadata)
{
    // HACK: Mark "debug" using `@luau` symbol instead
    if (auto it = globals.globalScope->bindings.find(Luau::AstName("debug")); it != globals.globalScope->bindings.end())
    {
        auto newDocumentationSymbol = it->second.documentationSymbol.value();
        replace(newDocumentationSymbol, "@roblox", "@luau");
        it->second.documentationSymbol = newDocumentationSymbol;
        fixDebugDocumentationSymbol(it->second.typeId, "debug");
    }

    // HACK: Mark "utf8" using `@luau` symbol instead
    if (auto it = globals.globalScope->bindings.find(Luau::AstName("utf8")); it != globals.globalScope->bindings.end())
    {
        auto newDocumentationSymbol = it->second.documentationSymbol.value();
        replace(newDocumentationSymbol, "@roblox", "@luau");
        it->second.documentationSymbol = newDocumentationSymbol;
        fixDebugDocumentationSymbol(it->second.typeId, "utf8");
    }

    // Extend Instance types
    if (auto objectType = globals.globalScope->lookupType("Object"))
    {
        if (auto* ctv = Luau::getMutable<Luau::ExternType>(objectType->type))
        {
            attachMagicFunctionSafe(ctv->props, "IsA", std::make_shared<MagicInstanceIsA>());
            attachMagicFunctionSafe(ctv->props, "GetPropertyChangedSignal", std::make_shared<MagicGetPropertyChangedSignal>());

            attachTagSafe(ctv->props, "IsA", "ClassNames");
            attachTagSafe(ctv->props, "GetPropertyChangedSignal", "Properties");
        }
    }

    if (auto instanceType = globals.globalScope->lookupType("Instance"))
    {
        if (auto* ctv = Luau::getMutable<Luau::ExternType>(instanceType->type))
        {
            Luau::attachTag(instanceType->type, Luau::kTypeofRootTag);

            attachMagicFunctionSafe(ctv->props, "FindFirstChildWhichIsA", std::make_shared<MagicInstanceFindFirstXWhichIsA>());
            attachMagicFunctionSafe(ctv->props, "FindFirstChildOfClass", std::make_shared<MagicInstanceFindFirstXWhichIsA>());
            attachMagicFunctionSafe(ctv->props, "FindFirstAncestorWhichIsA", std::make_shared<MagicInstanceFindFirstXWhichIsA>());
            attachMagicFunctionSafe(ctv->props, "FindFirstAncestorOfClass", std::make_shared<MagicInstanceFindFirstXWhichIsA>());
            attachMagicFunctionSafe(ctv->props, "Clone", std::make_shared<MagicInstanceClone>());

            // Autocomplete ClassNames for :IsA("") and counterparts
            attachTagSafe(ctv->props, "FindFirstChildWhichIsA", "ClassNames");
            attachTagSafe(ctv->props, "FindFirstChildOfClass", "ClassNames");
            attachTagSafe(ctv->props, "FindFirstAncestorWhichIsA", "ClassNames");
            attachTagSafe(ctv->props, "FindFirstAncestorOfClass", "ClassNames");

            // Go through all the defined classes and if they are a subclass of Instance then give them the
            // same metatable identity as Instance so that equality comparison works.
            // NOTE: This will OVERWRITE any metatables set on these classes!
            // We assume that all subclasses of instance don't have any metamethods
            for (auto& [_, ty] : globals.globalScope->exportedTypeBindings)
            {
                if (auto* c = Luau::getMutable<Luau::ExternType>(ty.type))
                {
                    // Check if the ctv is a subclass of instance
                    if (Luau::isSubclass(c, ctv))
                    {
                        c->metatable = ctv->metatable;
                    }
                }
            }
        }
    }

    std::optional<RobloxDefinitionsFileMetadata> robloxMetadata = metadata;

    // Attach onto Instance.new() and Instance.fromExisting()
    if (robloxMetadata.has_value() && !robloxMetadata->CREATABLE_INSTANCES.empty())
        if (auto instanceGlobal = globals.globalScope->lookup(Luau::AstName("Instance")))
            if (auto ttv = Luau::get<Luau::TableType>(instanceGlobal.value()))
            {
                if (auto newFunction = ttv->props.find("new");
                    newFunction != ttv->props.end() && Luau::get<Luau::FunctionType>(newFunction->second.type()))
                {

                    Luau::attachTag(newFunction->second.type(), "CreatableInstances");
                    Luau::attachMagicFunction(
                        newFunction->second.type(), std::make_shared<MagicTypeLookup>(robloxMetadata->CREATABLE_INSTANCES, "Invalid class name"));
                }

                if (auto newFunction = ttv->props.find("fromExisting");
                    newFunction != ttv->props.end() && Luau::get<Luau::FunctionType>(newFunction->second.type()))
                {
                    Luau::attachMagicFunction(newFunction->second.type(), std::make_shared<MagicInstanceFromExisting>());
                }
            }

    // Attach onto `game:GetService()`
    if (robloxMetadata.has_value() && !robloxMetadata->SERVICES.empty())
        if (auto serviceProviderType = globals.globalScope->lookupType("ServiceProvider"))
            if (auto* ctv = Luau::getMutable<Luau::ExternType>(serviceProviderType->type);
                ctv && ctv->props.find("GetService") != ctv->props.end() && Luau::get<Luau::FunctionType>(ctv->props["GetService"].type()))
            {
                Luau::attachTag(ctv->props["GetService"].type(), "Services");
                Luau::attachMagicFunction(
                    ctv->props["GetService"].type(), std::make_shared<MagicTypeLookup>(robloxMetadata->SERVICES, "Invalid service name"));
            }

    // Move Enums over as imported type bindings
    std::unordered_map<Luau::Name, Luau::TypeFun> enumTypes{};
    for (auto it = globals.globalScope->exportedTypeBindings.begin(); it != globals.globalScope->exportedTypeBindings.end();)
    {
        auto erase = false;
        auto ty = it->second.type;
        if (auto* ctv = Luau::getMutable<Luau::ExternType>(ty))
        {
            if (Luau::startsWith(ctv->name, "Enum"))
            {
                if (ctv->name == "EnumItem")
                {
                    attachMagicFunctionSafe(ctv->props, "IsA", std::make_shared<MagicEnumItemIsA>());
                    attachTagSafe(ctv->props, "IsA", "Enums");
                }
                else if (ctv->name != "Enum" && ctv->name != "Enums")
                {
                    // Erase the "Enum" at the start
                    ctv->name = ctv->name.substr(4);

                    // Move the enum over to the imported types if it is not internal, otherwise rename the type
                    if (endsWith(ctv->name, "_INTERNAL"))
                    {
                        ctv->name.erase(ctv->name.rfind("_INTERNAL"), 9);
                    }
                    else
                    {
                        enumTypes.emplace(ctv->name, it->second);
                        // Erase the metatable for the type, so it can be used in comparison
                    }

                    // Update the documentation symbol
                    Luau::asMutable(ty)->documentationSymbol = "@roblox/enum/" + ctv->name;
                    for (auto& [name, prop] : ctv->props)
                    {
                        prop.documentationSymbol = "@roblox/enum/" + ctv->name + "." + name;
                        Luau::attachTag(prop, "EnumItem");
                    }

                    // Prefix the name (after it has been placed into enumTypes) with "Enum."
                    ctv->name = "Enum." + ctv->name;

                    erase = true;
                }

                // Erase the metatable from the type to allow comparison
                ctv->metatable = std::nullopt;
            }
        }

        if (erase)
            it = globals.globalScope->exportedTypeBindings.erase(it);
        else
            ++it;
    }
    globals.globalScope->importedTypeBindings.emplace("Enum", enumTypes);
}