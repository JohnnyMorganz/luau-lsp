#include "Platform/RobloxPlatform.hpp"

#include "Luau/BuiltinDefinitions.h"
#include "Luau/ConstraintSolver.h"
#include "Luau/TypeInfer.h"

// Magic function for `Instance:IsA("ClassName")` predicate
std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionInstanceIsA(Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope,
    const Luau::AstExprCall& expr, const Luau::WithPredicate<Luau::TypePackId>& withPredicate)
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

static bool dcrMagicFunctionInstanceIsA(Luau::MagicFunctionCallContext context)
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

void dcrMagicRefinementInstanceIsA(const Luau::MagicRefinementContext& context)
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
std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionInstanceClone(Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope,
    const Luau::AstExprCall& expr, const Luau::WithPredicate<Luau::TypePackId>& withPredicate)
{
    auto index = expr.func->as<Luau::AstExprIndexName>();
    if (!index)
        return std::nullopt;

    Luau::TypeArena& arena = typeChecker.currentModule->internalTypes;
    auto instanceType = typeChecker.checkExpr(scope, *index->expr);
    return Luau::WithPredicate<Luau::TypePackId>{arena.addTypePack({instanceType.type})};
}

static bool dcrMagicFunctionInstanceClone(Luau::MagicFunctionCallContext context)
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

static std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionInstanceFromExisting(Luau::TypeChecker& typeChecker,
    const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, const Luau::WithPredicate<Luau::TypePackId>& withPredicate)
{
    if (expr.args.size < 1)
        return std::nullopt;

    Luau::TypeArena& arena = typeChecker.currentModule->internalTypes;
    auto instanceType = typeChecker.checkExpr(scope, *expr.args.data[0]);
    return Luau::WithPredicate<Luau::TypePackId>{arena.addTypePack({instanceType.type})};
}

static bool dcrMagicFunctionInstanceFromExisting(Luau::MagicFunctionCallContext context)
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
std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionFindFirstXWhichIsA(Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope,
    const Luau::AstExprCall& expr, const Luau::WithPredicate<Luau::TypePackId>& withPredicate)
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

static bool dcrMagicFunctionFindFirstXWhichIsA(Luau::MagicFunctionCallContext context)
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
std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionEnumItemIsA(Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope,
    const Luau::AstExprCall& expr, const Luau::WithPredicate<Luau::TypePackId>& withPredicate)
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

static bool dcrMagicFunctionEnumItemIsA(Luau::MagicFunctionCallContext context)
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

static void dcrMagicRefinementEnumItemIsA(const Luau::MagicRefinementContext& context)
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
static std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionGetPropertyChangedSignal(Luau::TypeChecker& typeChecker,
    const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, const Luau::WithPredicate<Luau::TypePackId>& withPredicate)
{
    if (expr.args.size != 1)
        return std::nullopt;

    auto index = expr.func->as<Luau::AstExprIndexName>();
    auto str = expr.args.data[0]->as<Luau::AstExprConstantString>();
    if (!index || !str)
        return std::nullopt;


    auto instanceType = typeChecker.checkExpr(scope, *index->expr);
    auto ctv = Luau::get<Luau::ClassType>(Luau::follow(instanceType.type));
    if (!ctv)
        return std::nullopt;

    std::string property(str->value.data, str->value.size);
    if (!Luau::lookupClassProp(ctv, property))
    {
        typeChecker.reportError(Luau::TypeError{expr.args.data[0]->location, Luau::UnknownProperty{instanceType.type, property}});
        return std::nullopt;
    }

    return std::nullopt;
}

static bool dcrMagicFunctionGetPropertyChangedSignal(Luau::MagicFunctionCallContext context)
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

    auto ctv = Luau::get<Luau::ClassType>(Luau::follow(selfTy));
    if (!ctv)
        return false;

    std::string property(str->value.data, str->value.size);
    if (!Luau::lookupClassProp(ctv, property))
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

static Luau::MagicFunction createMagicFunctionTypeLookup(const std::vector<std::string>& lookupList, const std::string& errorMessagePrefix)
{
    return [lookupList, errorMessagePrefix](Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr,
               const Luau::WithPredicate<Luau::TypePackId>& withPredicate) -> std::optional<Luau::WithPredicate<Luau::TypePackId>>
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
                typeChecker.reportError(
                    Luau::TypeError{expr.args.data[0]->location, Luau::GenericError{errorMessagePrefix + " '" + className + "'"}});
            }
        }

        return std::nullopt;
    };
}

static auto createDcrMagicFunctionTypeLookup(const std::vector<std::string>& lookupList, const std::string& errorMessagePrefix)
{
    return [lookupList, errorMessagePrefix](Luau::MagicFunctionCallContext context) -> bool
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
        if (auto* ctv = Luau::getMutable<Luau::ClassType>(objectType->type))
        {
            Luau::attachMagicFunction(ctv->props["IsA"].type(), magicFunctionInstanceIsA);
            Luau::attachMagicFunction(ctv->props["GetPropertyChangedSignal"].type(), magicFunctionGetPropertyChangedSignal);

            Luau::attachDcrMagicRefinement(ctv->props["IsA"].type(), dcrMagicRefinementInstanceIsA);
            Luau::attachDcrMagicFunction(ctv->props["IsA"].type(), dcrMagicFunctionInstanceIsA);
            Luau::attachDcrMagicFunction(ctv->props["GetPropertyChangedSignal"].type(), dcrMagicFunctionGetPropertyChangedSignal);

            Luau::attachTag(ctv->props["IsA"].type(), "ClassNames");
            Luau::attachTag(ctv->props["GetPropertyChangedSignal"].type(), "Properties");
        }
    }

    if (auto instanceType = globals.globalScope->lookupType("Instance"))
    {
        if (auto* ctv = Luau::getMutable<Luau::ClassType>(instanceType->type))
        {
            Luau::attachMagicFunction(ctv->props["FindFirstChildWhichIsA"].type(), magicFunctionFindFirstXWhichIsA);
            Luau::attachMagicFunction(ctv->props["FindFirstChildOfClass"].type(), magicFunctionFindFirstXWhichIsA);
            Luau::attachMagicFunction(ctv->props["FindFirstAncestorWhichIsA"].type(), magicFunctionFindFirstXWhichIsA);
            Luau::attachMagicFunction(ctv->props["FindFirstAncestorOfClass"].type(), magicFunctionFindFirstXWhichIsA);
            Luau::attachMagicFunction(ctv->props["Clone"].type(), magicFunctionInstanceClone);

            // TODO: clip once Object type is used everywhere
            if (ctv->props.find("IsA") != ctv->props.end()) {
                Luau::attachMagicFunction(ctv->props["IsA"].type(), magicFunctionInstanceIsA);
                Luau::attachDcrMagicRefinement(ctv->props["IsA"].type(), dcrMagicRefinementInstanceIsA);
                Luau::attachDcrMagicFunction(ctv->props["IsA"].type(), dcrMagicFunctionInstanceIsA);
                Luau::attachTag(ctv->props["IsA"].type(), "ClassNames");
            }
            if (ctv->props.find("GetPropertyChangedSignal") != ctv->props.end()) {
                Luau::attachMagicFunction(ctv->props["GetPropertyChangedSignal"].type(), magicFunctionGetPropertyChangedSignal);
                Luau::attachDcrMagicFunction(ctv->props["GetPropertyChangedSignal"].type(), dcrMagicFunctionGetPropertyChangedSignal);
                Luau::attachTag(ctv->props["GetPropertyChangedSignal"].type(), "Properties");
            }

            Luau::attachDcrMagicFunction(ctv->props["FindFirstChildWhichIsA"].type(), dcrMagicFunctionFindFirstXWhichIsA);
            Luau::attachDcrMagicFunction(ctv->props["FindFirstChildOfClass"].type(), dcrMagicFunctionFindFirstXWhichIsA);
            Luau::attachDcrMagicFunction(ctv->props["FindFirstAncestorWhichIsA"].type(), dcrMagicFunctionFindFirstXWhichIsA);
            Luau::attachDcrMagicFunction(ctv->props["FindFirstAncestorOfClass"].type(), dcrMagicFunctionFindFirstXWhichIsA);
            Luau::attachDcrMagicFunction(ctv->props["Clone"].type(), dcrMagicFunctionInstanceClone);

            // Autocomplete ClassNames for :IsA("") and counterparts
            Luau::attachTag(ctv->props["FindFirstChildWhichIsA"].type(), "ClassNames");
            Luau::attachTag(ctv->props["FindFirstChildOfClass"].type(), "ClassNames");
            Luau::attachTag(ctv->props["FindFirstAncestorWhichIsA"].type(), "ClassNames");
            Luau::attachTag(ctv->props["FindFirstAncestorOfClass"].type(), "ClassNames");

            // Go through all the defined classes and if they are a subclass of Instance then give them the
            // same metatable identity as Instance so that equality comparison works.
            // NOTE: This will OVERWRITE any metatables set on these classes!
            // We assume that all subclasses of instance don't have any metamethods
            for (auto& [_, ty] : globals.globalScope->exportedTypeBindings)
            {
                if (auto* c = Luau::getMutable<Luau::ClassType>(ty.type))
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
                        newFunction->second.type(), createMagicFunctionTypeLookup(robloxMetadata->CREATABLE_INSTANCES, "Invalid class name"));
                    Luau::attachDcrMagicFunction(
                        newFunction->second.type(), createDcrMagicFunctionTypeLookup(robloxMetadata->CREATABLE_INSTANCES, "Invalid class name"));
                }

                if (auto newFunction = ttv->props.find("fromExisting");
                    newFunction != ttv->props.end() && Luau::get<Luau::FunctionType>(newFunction->second.type()))
                {
                    Luau::attachMagicFunction(newFunction->second.type(), magicFunctionInstanceFromExisting);
                    Luau::attachDcrMagicFunction(newFunction->second.type(), dcrMagicFunctionInstanceFromExisting);
                }
            }

    // Attach onto `game:GetService()`
    if (robloxMetadata.has_value() && !robloxMetadata->SERVICES.empty())
        if (auto serviceProviderType = globals.globalScope->lookupType("ServiceProvider"))
            if (auto* ctv = Luau::getMutable<Luau::ClassType>(serviceProviderType->type);
                ctv && Luau::get<Luau::FunctionType>(ctv->props["GetService"].type()))
            {
                Luau::attachTag(ctv->props["GetService"].type(), "Services");
                Luau::attachMagicFunction(
                    ctv->props["GetService"].type(), createMagicFunctionTypeLookup(robloxMetadata->SERVICES, "Invalid service name"));
                Luau::attachDcrMagicFunction(
                    ctv->props["GetService"].type(), createDcrMagicFunctionTypeLookup(robloxMetadata->SERVICES, "Invalid service name"));
            }

    // Move Enums over as imported type bindings
    std::unordered_map<Luau::Name, Luau::TypeFun> enumTypes{};
    for (auto it = globals.globalScope->exportedTypeBindings.begin(); it != globals.globalScope->exportedTypeBindings.end();)
    {
        auto erase = false;
        auto ty = it->second.type;
        if (auto* ctv = Luau::getMutable<Luau::ClassType>(ty))
        {
            if (Luau::startsWith(ctv->name, "Enum"))
            {
                if (ctv->name == "EnumItem")
                {
                    Luau::attachMagicFunction(ctv->props["IsA"].type(), magicFunctionEnumItemIsA);
                    Luau::attachDcrMagicFunction(ctv->props["IsA"].type(), dcrMagicFunctionEnumItemIsA);
                    Luau::attachDcrMagicRefinement(ctv->props["IsA"].type(), dcrMagicRefinementEnumItemIsA);
                    Luau::attachTag(ctv->props["IsA"].type(), "Enums");
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