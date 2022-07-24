#include <climits>
#include "Luau/BuiltinDefinitions.h"
#include "Luau/ToString.h"
#include "Luau/Transpiler.h"
#include "LSP/LuauExt.hpp"
#include "LSP/Utils.hpp"

LUAU_FASTFLAG(LuauTypeMismatchModuleNameResolution)

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

std::optional<std::string> getTypeName(Luau::TypeId typeId)
{
    auto ty = Luau::follow(typeId);
    if (auto typeName = Luau::getName(ty))
    {
        return *typeName;
    }
    else if (auto parentClass = Luau::get<Luau::ClassTypeVar>(ty))
    {
        return parentClass->name;
    }
    // if (auto parentUnion = Luau::get<UnionTypeVar>(ty))
    // {
    //     return returnFirstNonnullOptionOfType<ClassTypeVar>(parentUnion);
    // }
    return std::nullopt;
}

Luau::TypeId makeLazyInstanceType(Luau::TypeArena& arena, const Luau::ScopePtr& globalScope, const SourceNodePtr& node,
    std::optional<Luau::TypeId> parent, std::optional<Luau::TypeId> baseClass)
{
    Luau::LazyTypeVar ltv;
    ltv.thunk = [&arena, globalScope, node, parent, baseClass]()
    {
        // TODO: we should cache created instance types and reuse them where possible

        // Handle if the node is no longer valid
        if (!node)
            return Luau::getSingletonTypes().anyType;

        auto instanceTy = globalScope->lookupType("Instance");
        if (!instanceTy)
            return Luau::getSingletonTypes().anyType;

        // Look up the base class instance
        Luau::TypeId baseTypeId;
        if (baseClass)
            baseTypeId = *baseClass;
        else if (auto foundId = getTypeIdForClass(globalScope, node->className))
            baseTypeId = *foundId;
        else
            return Luau::getSingletonTypes().anyType;

        // Point the metatable to the metatable of "Instance" so that we allow equality
        std::optional<Luau::TypeId> instanceMetaIdentity;
        if (auto* ctv = Luau::get<Luau::ClassTypeVar>(instanceTy->type))
            instanceMetaIdentity = ctv->metatable;

        // Create the ClassTypeVar representing the instance
        std::string typeName = getTypeName(baseTypeId).value_or(node->name);
        Luau::ClassTypeVar ctv{typeName, {}, baseTypeId, instanceMetaIdentity, {}, {}, "@roblox"};
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

            // Add FindFirstAncestor and FindFirstChild
            if (auto instanceType = getTypeIdForClass(globalScope, "Instance"))
            {
                auto findFirstAncestorFunction = Luau::makeFunction(arena, typeId, {Luau::getSingletonTypes().stringType}, {"name"}, {*instanceType});
                Luau::attachMagicFunction(findFirstAncestorFunction,
                    [node](Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr,
                        Luau::WithPredicate<Luau::TypePackId> withPredicate) -> std::optional<Luau::WithPredicate<Luau::TypePackId>>
                    {
                        if (expr.args.size < 1)
                            return std::nullopt;

                        auto str = expr.args.data[0]->as<Luau::AstExprConstantString>();
                        if (!str)
                            return std::nullopt;

                        // This is a O(n) search, not great!
                        if (auto ancestor = node->findAncestor(std::string(str->value.data, str->value.size)))
                        {
                            return Luau::WithPredicate<Luau::TypePackId>{
                                typeChecker.globalTypes.addTypePack({makeLazyInstanceType(typeChecker.globalTypes, scope, *ancestor, std::nullopt)})};
                        }

                        return std::nullopt;
                    });
                ctv->props["FindFirstAncestor"] = Luau::makeProperty(findFirstAncestorFunction, "@roblox/globaltype/Instance.FindFirstAncestor");

                auto findFirstChildFunction = Luau::makeFunction(arena, typeId, {Luau::getSingletonTypes().stringType}, {"name"}, {*instanceType});
                Luau::attachMagicFunction(findFirstChildFunction,
                    [node, typeId](Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr,
                        Luau::WithPredicate<Luau::TypePackId> withPredicate) -> std::optional<Luau::WithPredicate<Luau::TypePackId>>
                    {
                        if (expr.args.size < 1)
                            return std::nullopt;

                        auto str = expr.args.data[0]->as<Luau::AstExprConstantString>();
                        if (!str)
                            return std::nullopt;

                        if (auto child = node->findChild(std::string(str->value.data, str->value.size)))
                        {
                            return Luau::WithPredicate<Luau::TypePackId>{
                                typeChecker.globalTypes.addTypePack({makeLazyInstanceType(typeChecker.globalTypes, scope, *child, typeId)})};
                        }

                        return std::nullopt;
                    });
                ctv->props["FindFirstChild"] = Luau::makeProperty(findFirstChildFunction, "@roblox/globaltype/Instance.FindFirstChild");
            }
        }
        return typeId;
    };

    return arena.addType(std::move(ltv));
}

// Magic function for `Instance:IsA("ClassName")` predicate
std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionInstanceIsA(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId> withPredicate)
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
    return Luau::WithPredicate<Luau::TypePackId>{booleanPack, {Luau::IsAPredicate{std::move(*lvalue), expr.location, tfun->type}}};
}

// Magic function for `instance:Clone()`, so that we return the exact subclass that `instance` is, rather than just a generic Instance
std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionInstanceClone(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId> withPredicate)
{
    auto index = expr.func->as<Luau::AstExprIndexName>();
    if (!index)
        return std::nullopt;

    Luau::TypeArena& arena = typeChecker.currentModule->internalTypes;
    auto instanceType = typeChecker.checkExpr(scope, *index->expr);
    return Luau::WithPredicate<Luau::TypePackId>{arena.addTypePack({instanceType.type})};
}

// Magic function for `Instance:FindFirstChildWhichIsA("ClassName")` and friends
std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionFindFirstXWhichIsA(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId> withPredicate)
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
    return Luau::WithPredicate<Luau::TypePackId>{typeChecker.globalTypes.addTypePack({nillableClass})};
}

// Magic function for `EnumItem:IsA("EnumType")` predicate
std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionEnumItemIsA(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId> withPredicate)
{
    if (expr.args.size != 1)
        return std::nullopt;

    auto index = expr.func->as<Luau::AstExprIndexName>();
    auto str = expr.args.data[0]->as<Luau::AstExprConstantString>();
    if (!index || !str)
        return std::nullopt;

    std::optional<Luau::LValue> lvalue = tryGetLValue(*index->expr);
    std::optional<Luau::TypeFun> tfun = scope->lookupImportedType("Enum", std::string(str->value.data, str->value.size));
    if (!lvalue || !tfun)
        return std::nullopt;

    Luau::TypePackId booleanPack = typeChecker.globalTypes.addTypePack({typeChecker.booleanType});
    return Luau::WithPredicate<Luau::TypePackId>{booleanPack, {Luau::IsAPredicate{std::move(*lvalue), expr.location, tfun->type}}};
}

// TODO: expressiveTypes is used because of a Luau issue where we can't cast a most specific Instance type (which we create here)
// to another type. For the time being, we therefore make all our DataModel instance types marked as "any".
// Remove this once Luau has improved
void registerInstanceTypes(Luau::TypeChecker& typeChecker, const WorkspaceFileResolver& fileResolver, bool expressiveTypes)
{
    // Extend the types from the sourcemap
    // Extend globally registered types with Instance information
    if (fileResolver.rootSourceNode)
    {
        if (fileResolver.rootSourceNode->className == "DataModel")
        {
            Luau::unfreeze(typeChecker.globalTypes);
            // Add instance information for the DataModel tree
            for (const auto& service : fileResolver.rootSourceNode->children)
            {
                auto serviceName = service->className; // We know it must be a service of the same class name
                if (auto serviceType = typeChecker.globalScope->lookupType(serviceName))
                {
                    if (Luau::ClassTypeVar* ctv = Luau::getMutable<Luau::ClassTypeVar>(serviceType->type))
                    {
                        // Clear out all the old registered children
                        for (auto it = ctv->props.begin(); it != ctv->props.end();)
                        {
                            if (hasTag(it->second, "@sourcemap-generated"))
                                it = ctv->props.erase(it);
                            else
                                ++it;
                        }


                        // Extend the props to include the children
                        for (const auto& child : service->children)
                        {
                            Luau::Property property{
                                types::makeLazyInstanceType(typeChecker.globalTypes, typeChecker.globalScope, child, serviceType->type),
                                /* deprecated */ false,
                                /* deprecatedSuggestion */ {},
                                /* location */ std::nullopt,
                                /* tags */ {"@sourcemap-generated"},
                                /* documentationSymbol*/ std::nullopt,
                            };
                            ctv->props[child->name] = property;
                        }
                    }
                }
            }

            // Add containers to player and copy over instances
            // Player.Character should contain StarterCharacter instances
            if (auto playerType = typeChecker.globalScope->lookupType("Player"))
            {
                if (auto* ctv = Luau::getMutable<Luau::ClassTypeVar>(playerType->type))
                {
                    // Player.PlayerGui should contain StarterGui instances
                    if (auto playerGuiType = typeChecker.globalScope->lookupType("PlayerGui"))
                    {
                        ctv->props["PlayerGui"] = Luau::makeProperty(playerGuiType->type);
                        if (auto starterGui = fileResolver.rootSourceNode->findChild("StarterGui"))
                        {
                            ctv->props["PlayerGui"] = Luau::makeProperty(types::makeLazyInstanceType(
                                typeChecker.globalTypes, typeChecker.globalScope, starterGui.value(), std::nullopt, playerGuiType->type));
                        }
                    }

                    // Player.StarterGear should contain StarterPack instances
                    if (auto starterGearType = typeChecker.globalScope->lookupType("StarterGear"))
                    {
                        ctv->props["StarterGear"] = Luau::makeProperty(starterGearType->type);
                        if (auto starterPack = fileResolver.rootSourceNode->findChild("StarterPack"))
                        {
                            ctv->props["StarterGear"] = Luau::makeProperty(types::makeLazyInstanceType(
                                typeChecker.globalTypes, typeChecker.globalScope, starterPack.value(), std::nullopt, starterGearType->type));
                        }
                    }

                    // Player.Backpack should be defined
                    if (auto backpackType = typeChecker.globalScope->lookupType("Backpack"))
                    {
                        ctv->props["Backpack"] = Luau::makeProperty(backpackType->type);
                        // TODO: should we duplicate StarterPack into here as well? Is that a reasonable assumption to make?
                    }

                    // Player.PlayerScripts should contain StarterPlayerScripts instances
                    if (auto playerScriptsType = typeChecker.globalScope->lookupType("PlayerScripts"))
                    {
                        ctv->props["PlayerScripts"] = Luau::makeProperty(playerScriptsType->type);
                        if (auto starterPlayer = fileResolver.rootSourceNode->findChild("StarterPlayer"))
                        {
                            if (auto starterPlayerScripts = starterPlayer.value()->findChild("StarterPlayerScripts"))
                            {
                                ctv->props["PlayerScripts"] = Luau::makeProperty(types::makeLazyInstanceType(typeChecker.globalTypes,
                                    typeChecker.globalScope, starterPlayerScripts.value(), std::nullopt, playerScriptsType->type));
                            }
                        }
                    }
                }
            }


            Luau::freeze(typeChecker.globalTypes);
        }

        // Prepare module scope so that we can dynamically reassign the type of "script" to retrieve instance info
        typeChecker.prepareModuleScope = [&, expressiveTypes](const Luau::ModuleName& name, const Luau::ScopePtr& scope)
        {
            // TODO: we hope to remove these in future!
            if (!expressiveTypes)
            {
                scope->bindings[Luau::AstName("script")] = Luau::Binding{Luau::getSingletonTypes().anyType};
                scope->bindings[Luau::AstName("workspace")] = Luau::Binding{Luau::getSingletonTypes().anyType};
                scope->bindings[Luau::AstName("game")] = Luau::Binding{Luau::getSingletonTypes().anyType};
            }

            if (auto node =
                    fileResolver.isVirtualPath(name) ? fileResolver.getSourceNodeFromVirtualPath(name) : fileResolver.getSourceNodeFromRealPath(name))
            {
                // HACK: we need a way to get the typeArena for the module, but I don't know how
                // we can see that moduleScope->returnType is assigned before prepareModuleScope is called in TypeInfer, so we could try it
                // this way...
                LUAU_ASSERT(scope->returnType);
                auto typeArena = scope->returnType->owningArena;
                LUAU_ASSERT(typeArena);

                if (expressiveTypes)
                    scope->bindings[Luau::AstName("script")] =
                        Luau::Binding{types::makeLazyInstanceType(*typeArena, scope, node.value(), std::nullopt)};
            }
        };
    }
}

Luau::LoadDefinitionFileResult registerDefinitions(Luau::TypeChecker& typeChecker, const std::filesystem::path& definitionsFile)
{
    if (auto definitions = readFile(definitionsFile))
    {
        auto loadResult = Luau::loadDefinitionFile(typeChecker, typeChecker.globalScope, *definitions, "@roblox");
        if (!loadResult.success)
        {
            return loadResult;
        }

        // Extend Instance types
        if (auto instanceType = typeChecker.globalScope->lookupType("Instance"))
        {
            if (auto* ctv = Luau::getMutable<Luau::ClassTypeVar>(instanceType->type))
            {
                Luau::attachMagicFunction(ctv->props["IsA"].type, types::magicFunctionInstanceIsA);
                Luau::attachMagicFunction(ctv->props["FindFirstChildWhichIsA"].type, types::magicFunctionFindFirstXWhichIsA);
                Luau::attachMagicFunction(ctv->props["FindFirstChildOfClass"].type, types::magicFunctionFindFirstXWhichIsA);
                Luau::attachMagicFunction(ctv->props["FindFirstAncestorWhichIsA"].type, types::magicFunctionFindFirstXWhichIsA);
                Luau::attachMagicFunction(ctv->props["FindFirstAncestorOfClass"].type, types::magicFunctionFindFirstXWhichIsA);
                Luau::attachMagicFunction(ctv->props["Clone"].type, types::magicFunctionInstanceClone);

                // Go through all the defined classes and if they are a subclass of Instance then give them the
                // same metatable identity as Instance so that equality comparison works.
                // NOTE: This will OVERWRITE any metatables set on these classes!
                // We assume that all subclasses of instance don't have any metamethaods
                for (auto& [_, ty] : typeChecker.globalScope->exportedTypeBindings)
                {
                    if (auto* c = Luau::getMutable<Luau::ClassTypeVar>(ty.type))
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

        // Move Enums over as imported type bindings
        std::unordered_map<Luau::Name, Luau::TypeFun> enumTypes;
        for (auto it = typeChecker.globalScope->exportedTypeBindings.begin(); it != typeChecker.globalScope->exportedTypeBindings.end();)
        {
            auto erase = false;
            auto ty = it->second.type;
            if (auto* ctv = Luau::getMutable<Luau::ClassTypeVar>(ty))
            {
                if (Luau::startsWith(ctv->name, "Enum"))
                {
                    if (ctv->name == "EnumItem")
                    {
                        Luau::attachMagicFunction(ctv->props["IsA"].type, types::magicFunctionEnumItemIsA);
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
                            // Erase the metatable for the type so it can be used in comparison
                        }

                        // Update the documentation symbol
                        Luau::asMutable(ty)->documentationSymbol = "@roblox/enum/" + ctv->name;
                        for (auto& [name, prop] : ctv->props)
                        {
                            prop.documentationSymbol = "@roblox/enum/" + ctv->name + "." + name;
                        }

                        // Prefix the name (after its been placed into enumTypes) with "Enum."
                        ctv->name = "Enum." + ctv->name;

                        erase = true;
                    }

                    // Erase the metatable from the type to allow comparison
                    ctv->metatable = std::nullopt;
                }
            };

            if (erase)
                it = typeChecker.globalScope->exportedTypeBindings.erase(it);
            else
                ++it;
        }
        typeChecker.globalScope->importedTypeBindings.emplace("Enum", enumTypes);

        return loadResult;
    }
    else
    {
        return {false};
    }
}

using NameOrExpr = std::variant<std::string, Luau::AstExpr*>;

// Converts a FTV and function call to a nice string
// In the format "function NAME(args): ret"
std::string toStringNamedFunction(
    Luau::ModulePtr module, const Luau::FunctionTypeVar* ftv, const NameOrExpr nameOrFuncExpr, std::optional<Luau::ScopePtr> scope)
{
    Luau::ToStringOptions opts;
    opts.functionTypeArguments = true;
    opts.hideNamedFunctionTypeParameters = false;
    if (scope)
        opts.scope = *scope;
    auto functionString = Luau::toStringNamedFunction("", *ftv, opts);

    // HACK: remove all instances of "_: " from the function string
    // They don't look great, maybe we should upstream this as an option?
    replaceAll(functionString, "_: ", "");

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
        replaceAll(functionString, "_: ", "");
        // We can try and give a temporary base name from what we can infer by the index, and then attempt to improve it with proper information
        baseName = Luau::toString(indexName->expr);
        trim(baseName); // Trim it, because toString is probably not meant to be used in this context (it has whitespace)
    }
    else if (auto indexExpr = funcExpr->as<Luau::AstExprIndexExpr>())
    {
        parentIt = module->astTypes.find(indexExpr->expr);
        methodName = "[" + Luau::toString(indexExpr->index) + "]";
        // We can try and give a temporary base name from what we can infer by the index, and then attempt to improve it with proper information
        baseName = Luau::toString(indexExpr->expr);
        // Trim it, because toString is probably not meant to be used in this context (it has whitespace)
        trim(baseName);
    }

    if (!parentIt)
        return "function" + methodName + functionString;

    if (auto name = getTypeName(*parentIt))
        baseName = *name;

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
        if (auto mtable = Luau::get<Luau::TableTypeVar>(mt->metatable))
        {
            auto indexIt = mtable->props.find("__index");
            if (indexIt != mtable->props.end())
            {
                Luau::TypeId followed = Luau::follow(indexIt->second.type);
                if (Luau::get<Luau::TableTypeVar>(followed) || Luau::get<Luau::MetatableTypeVar>(followed))
                {
                    return lookupProp(followed, name);
                }
                else if (Luau::get<Luau::FunctionTypeVar>(followed))
                {
                    // TODO: can we handle an index function...?
                    return std::nullopt;
                }
            }
        }

        if (auto tbl = Luau::get<Luau::TableTypeVar>(mt->table))
        {
            if (tbl->props.find(name) != tbl->props.end())
            {
                return tbl->props.at(name);
            }
        }
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

lsp::Diagnostic createTypeErrorDiagnostic(const Luau::TypeError& error, Luau::FileResolver* fileResolver)
{
    std::string message;
    if (const Luau::SyntaxError* syntaxError = Luau::get_if<Luau::SyntaxError>(&error.data))
        message = "SyntaxError: " + syntaxError->message;
    else if (FFlag::LuauTypeMismatchModuleNameResolution)
        message = "TypeError: " + Luau::toString(error, Luau::TypeErrorToStringOptions{fileResolver});
    else
        message = "TypeError: " + Luau::toString(error);

    lsp::Diagnostic diagnostic;
    diagnostic.source = "Luau";
    diagnostic.code = error.code();
    diagnostic.message = message;
    diagnostic.severity = lsp::DiagnosticSeverity::Error;
    diagnostic.range = {convertPosition(error.location.begin), convertPosition(error.location.end)};
    diagnostic.codeDescription = {Uri::parse("https://luau-lang.org/typecheck")};
    return diagnostic;
}

lsp::Diagnostic createLintDiagnostic(const Luau::LintWarning& lint)
{
    std::string lintName = Luau::LintWarning::getName(lint.code);

    lsp::Diagnostic diagnostic;
    diagnostic.source = "Luau";
    diagnostic.code = lint.code;
    diagnostic.message = lintName + ": " + lint.text;
    diagnostic.severity = lsp::DiagnosticSeverity::Warning; // Configuration can convert this to an error
    diagnostic.range = {convertPosition(lint.location.begin), convertPosition(lint.location.end)};
    diagnostic.codeDescription = {Uri::parse("https://luau-lang.org/lint#" + toLower(lintName) + "-" + std::to_string(static_cast<int>(lint.code)))};

    if (lint.code == Luau::LintWarning::Code::Code_LocalUnused || lint.code == Luau::LintWarning::Code::Code_ImportUnused ||
        lint.code == Luau::LintWarning::Code::Code_FunctionUnused)
    {
        diagnostic.tags.emplace_back(lsp::DiagnosticTag::Unnecessary);
    }
    else if (lint.code == Luau::LintWarning::Code::Code_DeprecatedApi || lint.code == Luau::LintWarning::Code::Code_DeprecatedGlobal)
    {
        diagnostic.tags.emplace_back(lsp::DiagnosticTag::Deprecated);
    }

    return diagnostic;
}

lsp::Diagnostic createParseErrorDiagnostic(const Luau::ParseError& error)
{
    lsp::Diagnostic diagnostic;
    diagnostic.source = "Luau";
    diagnostic.code = "SyntaxError";
    diagnostic.message = "SyntaxError: " + error.getMessage();
    diagnostic.severity = lsp::DiagnosticSeverity::Error;
    diagnostic.range = {convertPosition(error.getLocation().begin), convertPosition(error.getLocation().end)};
    diagnostic.codeDescription = {Uri::parse("https://luau-lang.org/syntax")};
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

std::optional<Luau::Location> getLocation(Luau::TypeId type)
{
    type = follow(type);

    if (auto ftv = Luau::get<Luau::FunctionTypeVar>(type))
    {
        if (ftv->definition)
            return ftv->definition->originalNameLocation;
    }

    return std::nullopt;
}