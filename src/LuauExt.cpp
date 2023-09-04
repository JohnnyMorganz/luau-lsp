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

std::optional<std::string> getTypeName(Luau::TypeId typeId)
{
    std::optional<std::string> name = std::nullopt;
    auto ty = Luau::follow(typeId);

    if (auto typeName = Luau::getName(ty))
    {
        name = *typeName;
    }
    else if (auto mtv = Luau::get<Luau::MetatableType>(ty))
    {
        if (auto mtvName = Luau::getName(mtv->metatable))
            name = *mtvName;
    }
    else if (auto parentClass = Luau::get<Luau::ClassType>(ty))
    {
        name = parentClass->name;
    }
    // if (auto parentUnion = Luau::get<UnionType>(ty))
    // {
    //     return returnFirstNonnullOptionOfType<ClassType>(parentUnion);
    // }

    // strip synthetic typeof() for builtin tables
    if (name && name->compare(0, 7, "typeof(") == 0 && name->back() == ')')
        return name->substr(7, name->length() - 8);
    else
        return name;
}

// Retrieves the corresponding Luau type for a Sourcemap node
// If it does not yet exist, the type is produced
Luau::TypeId getSourcemapType(const Luau::GlobalTypes& globals, Luau::TypeArena& arena, const SourceNodePtr& node)
{
    // Gets the type corresponding to the sourcemap node if it exists
    // Make sure to use the correct ty version (base typeChecker vs autocomplete typeChecker)
    if (node->tys.find(&globals) != node->tys.end())
        return node->tys.at(&globals);

    Luau::LazyType ltv(
        [&globals, &arena, node](Luau::LazyType& ltv) -> void
        {
            // Check if the LTV already has an unwrapped type
            if (ltv.unwrapped.load())
                return;

            // Handle if the node is no longer valid
            if (!node)
            {
                ltv.unwrapped = globals.builtinTypes->anyType;
                return;
            }

            auto instanceTy = globals.globalScope->lookupType("Instance");
            if (!instanceTy)
            {
                ltv.unwrapped = globals.builtinTypes->anyType;
                return;
            }

            // Look up the base class instance
            Luau::TypeId baseTypeId = getTypeIdForClass(globals.globalScope, node->className).value_or(nullptr);
            if (!baseTypeId)
            {
                ltv.unwrapped = globals.builtinTypes->anyType;
                return;
            }

            // Point the metatable to the metatable of "Instance" so that we allow equality
            std::optional<Luau::TypeId> instanceMetaIdentity;
            if (auto* ctv = Luau::get<Luau::ClassType>(instanceTy->type))
                instanceMetaIdentity = ctv->metatable;

            // Create the ClassType representing the instance
            std::string typeName = getTypeName(baseTypeId).value_or(node->name);
            Luau::ClassType baseInstanceCtv{typeName, {}, baseTypeId, instanceMetaIdentity, {}, {}, "@roblox"};
            auto typeId = arena.addType(std::move(baseInstanceCtv));

            // Attach Parent and Children info
            // Get the mutable version of the type var
            if (auto* ctv = Luau::getMutable<Luau::ClassType>(typeId))
            {
                if (auto parentNode = node->parent.lock())
                    ctv->props["Parent"] = Luau::makeProperty(getSourcemapType(globals, arena, parentNode));

                // Add children as properties
                for (const auto& child : node->children)
                    ctv->props[child->name] = Luau::makeProperty(getSourcemapType(globals, arena, child));

                // Add FindFirstAncestor and FindFirstChild
                if (auto instanceType = getTypeIdForClass(globals.globalScope, "Instance"))
                {
                    auto findFirstAncestorFunction = Luau::makeFunction(arena, typeId, {globals.builtinTypes->stringType}, {"name"}, {*instanceType});

                    Luau::attachMagicFunction(findFirstAncestorFunction,
                        [&arena, &globals, node](Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr,
                            const Luau::WithPredicate<Luau::TypePackId>& withPredicate) -> std::optional<Luau::WithPredicate<Luau::TypePackId>>
                        {
                            if (expr.args.size < 1)
                                return std::nullopt;

                            auto str = expr.args.data[0]->as<Luau::AstExprConstantString>();
                            if (!str)
                                return std::nullopt;

                            // This is a O(n) search, not great!
                            if (auto ancestor = node->findAncestor(std::string(str->value.data, str->value.size)))
                            {
                                return Luau::WithPredicate<Luau::TypePackId>{arena.addTypePack({getSourcemapType(globals, arena, *ancestor)})};
                            }

                            return std::nullopt;
                        });
                    ctv->props["FindFirstAncestor"] = Luau::makeProperty(findFirstAncestorFunction, "@roblox/globaltype/Instance.FindFirstAncestor");

                    auto findFirstChildFunction = Luau::makeFunction(arena, typeId, {globals.builtinTypes->stringType}, {"name"}, {*instanceType});
                    Luau::attachMagicFunction(findFirstChildFunction,
                        [node, &arena, &globals](Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr,
                            const Luau::WithPredicate<Luau::TypePackId>& withPredicate) -> std::optional<Luau::WithPredicate<Luau::TypePackId>>
                        {
                            if (expr.args.size < 1)
                                return std::nullopt;

                            auto str = expr.args.data[0]->as<Luau::AstExprConstantString>();
                            if (!str)
                                return std::nullopt;

                            if (auto child = node->findChild(std::string(str->value.data, str->value.size)))
                                return Luau::WithPredicate<Luau::TypePackId>{arena.addTypePack({getSourcemapType(globals, arena, *child)})};

                            return std::nullopt;
                        });
                    ctv->props["FindFirstChild"] = Luau::makeProperty(findFirstChildFunction, "@roblox/globaltype/Instance.FindFirstChild");
                }
            }

            ltv.unwrapped = typeId;
            return;
        });
    auto ty = arena.addType(std::move(ltv));
    node->tys.insert_or_assign(&globals, ty);

    return ty;
}

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

// Magic function attached to `Instance.new(string) -> Instance`, where if the argument given is a string literal
// then we must error since we have hit the fallback value
static std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionInstanceNew(Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope,
    const Luau::AstExprCall& expr, const Luau::WithPredicate<Luau::TypePackId>& withPredicate)
{
    if (expr.args.size < 1)
        return std::nullopt;

    if (auto str = expr.args.data[0]->as<Luau::AstExprConstantString>())
        typeChecker.reportError(Luau::TypeError{
            expr.args.data[0]->location, Luau::GenericError{"Invalid class name '" + std::string(str->value.data, str->value.size) + "'"}});

    return std::nullopt;
}

void addChildrenToCTV(const Luau::GlobalTypes& globals, Luau::TypeArena& arena, const Luau::TypeId& ty, const SourceNodePtr& node)
{
    if (auto* ctv = Luau::getMutable<Luau::ClassType>(ty))
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
        for (const auto& child : node->children)
        {
            ctv->props[child->name] = Luau::Property{
                getSourcemapType(globals, arena, child),
                /* deprecated */ false,
                /* deprecatedSuggestion */ {},
                /* location */ std::nullopt,
                /* tags */ {"@sourcemap-generated"},
                /* documentationSymbol*/ std::nullopt,
            };
        }
    }
}

// TODO: expressiveTypes is used because of a Luau issue where we can't cast a most specific Instance type (which we create here)
// to another type. For the time being, we therefore make all our DataModel instance types marked as "any".
// Remove this once Luau has improved
void registerInstanceTypes(Luau::Frontend& frontend, const Luau::GlobalTypes& globals, Luau::TypeArena& arena,
    const WorkspaceFileResolver& fileResolver, bool expressiveTypes)
{
    if (!fileResolver.rootSourceNode)
        return;

    // Create a type for the root source node
    getSourcemapType(globals, arena, fileResolver.rootSourceNode);

    // Modify sourcemap types
    if (fileResolver.rootSourceNode->className == "DataModel")
    {
        // Mutate DataModel with its children
        if (auto dataModelType = globals.globalScope->lookupType("DataModel"))
            addChildrenToCTV(globals, arena, dataModelType->type, fileResolver.rootSourceNode);

        // Mutate globally-registered Services to include children information (so it's available through :GetService)
        for (const auto& service : fileResolver.rootSourceNode->children)
        {
            auto serviceName = service->className; // We know it must be a service of the same class name
            if (auto serviceType = globals.globalScope->lookupType(serviceName))
                addChildrenToCTV(globals, arena, serviceType->type, service);
        }

        // Add containers to player and copy over instances
        // TODO: Player.Character should contain StarterCharacter instances
        if (auto playerType = globals.globalScope->lookupType("Player"))
        {
            if (auto* ctv = Luau::getMutable<Luau::ClassType>(playerType->type))
            {
                // Player.Backpack should be defined
                if (auto backpackType = globals.globalScope->lookupType("Backpack"))
                {
                    ctv->props["Backpack"] = Luau::makeProperty(backpackType->type);
                    // TODO: should we duplicate StarterPack into here as well? Is that a reasonable assumption to make?
                }

                // Player.PlayerGui should contain StarterGui instances
                if (auto playerGuiType = globals.globalScope->lookupType("PlayerGui"))
                {
                    if (auto starterGui = fileResolver.rootSourceNode->findChild("StarterGui"))
                        addChildrenToCTV(globals, arena, playerGuiType->type, *starterGui);
                    ctv->props["PlayerGui"] = Luau::makeProperty(playerGuiType->type);
                }

                // Player.StarterGear should contain StarterPack instances
                if (auto starterGearType = globals.globalScope->lookupType("StarterGear"))
                {
                    if (auto starterPack = fileResolver.rootSourceNode->findChild("StarterPack"))
                        addChildrenToCTV(globals, arena, starterGearType->type, *starterPack);

                    ctv->props["StarterGear"] = Luau::makeProperty(starterGearType->type);
                }

                // Player.PlayerScripts should contain StarterPlayerScripts instances
                if (auto playerScriptsType = globals.globalScope->lookupType("PlayerScripts"))
                {
                    if (auto starterPlayer = fileResolver.rootSourceNode->findChild("StarterPlayer"))
                    {
                        if (auto starterPlayerScripts = starterPlayer.value()->findChild("StarterPlayerScripts"))
                        {
                            addChildrenToCTV(globals, arena, playerScriptsType->type, *starterPlayerScripts);
                        }
                    }
                    ctv->props["PlayerScripts"] = Luau::makeProperty(playerScriptsType->type);
                }
            }
        }
    }

    // Prepare module scope so that we can dynamically reassign the type of "script" to retrieve instance info
    frontend.prepareModuleScope = [&frontend, &fileResolver, &arena, expressiveTypes](
                                      const Luau::ModuleName& name, const Luau::ScopePtr& scope, bool forAutocomplete)
    {
        Luau::GlobalTypes& globals = forAutocomplete ? frontend.globalsForAutocomplete : frontend.globals;

        // TODO: we hope to remove these in future!
        if (!expressiveTypes && !forAutocomplete)
        {
            scope->bindings[Luau::AstName("script")] = Luau::Binding{globals.builtinTypes->anyType};
            scope->bindings[Luau::AstName("workspace")] = Luau::Binding{globals.builtinTypes->anyType};
            scope->bindings[Luau::AstName("game")] = Luau::Binding{globals.builtinTypes->anyType};
        }

        if (expressiveTypes || forAutocomplete)
            if (auto node =
                    fileResolver.isVirtualPath(name) ? fileResolver.getSourceNodeFromVirtualPath(name) : fileResolver.getSourceNodeFromRealPath(name))
                scope->bindings[Luau::AstName("script")] = Luau::Binding{getSourcemapType(globals, arena, node.value())};
    };
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

static auto createMagicFunctionTypeLookup(const std::vector<std::string>& lookupList, const std::string& errorMessagePrefix)
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

std::optional<DefinitionsFileMetadata> parseDefinitionsFileMetadata(const std::string& definitions)
{
    auto firstLine = getFirstLine(definitions);
    if (Luau::startsWith(firstLine, "--#METADATA#"))
    {
        firstLine = firstLine.substr(12);
        return json::parse(firstLine);
    }
    return std::nullopt;
}

Luau::LoadDefinitionFileResult registerDefinitions(Luau::Frontend& frontend, Luau::GlobalTypes& globals, const std::string& definitions,
    bool typeCheckForAutocomplete, std::optional<DefinitionsFileMetadata> metadata)
{
    // TODO: packageName shouldn't just be "@roblox"
    auto loadResult =
        frontend.loadDefinitionFile(globals, globals.globalScope, definitions, "@roblox", /* captureComments = */ false, typeCheckForAutocomplete);
    if (!loadResult.success)
        return loadResult;

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
    if (auto instanceType = globals.globalScope->lookupType("Instance"))
    {
        if (auto* ctv = Luau::getMutable<Luau::ClassType>(instanceType->type))
        {
            Luau::attachMagicFunction(ctv->props["IsA"].type(), types::magicFunctionInstanceIsA);
            Luau::attachMagicFunction(ctv->props["FindFirstChildWhichIsA"].type(), types::magicFunctionFindFirstXWhichIsA);
            Luau::attachMagicFunction(ctv->props["FindFirstChildOfClass"].type(), types::magicFunctionFindFirstXWhichIsA);
            Luau::attachMagicFunction(ctv->props["FindFirstAncestorWhichIsA"].type(), types::magicFunctionFindFirstXWhichIsA);
            Luau::attachMagicFunction(ctv->props["FindFirstAncestorOfClass"].type(), types::magicFunctionFindFirstXWhichIsA);
            Luau::attachMagicFunction(ctv->props["Clone"].type(), types::magicFunctionInstanceClone);
            Luau::attachMagicFunction(ctv->props["GetPropertyChangedSignal"].type(), magicFunctionGetPropertyChangedSignal);

            // Autocomplete ClassNames for :IsA("") and counterparts
            Luau::attachTag(ctv->props["IsA"].type(), "ClassNames");
            Luau::attachTag(ctv->props["FindFirstChildWhichIsA"].type(), "ClassNames");
            Luau::attachTag(ctv->props["FindFirstChildOfClass"].type(), "ClassNames");
            Luau::attachTag(ctv->props["FindFirstAncestorWhichIsA"].type(), "ClassNames");
            Luau::attachTag(ctv->props["FindFirstAncestorOfClass"].type(), "ClassNames");

            // Autocomplete Properties for :GetPropertyChangedSignal("")
            Luau::attachTag(ctv->props["GetPropertyChangedSignal"].type(), "Properties");

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

    // Attach onto Instance.new()
    if (auto instanceGlobal = globals.globalScope->lookup(Luau::AstName("Instance")))
        if (auto ttv = Luau::get<Luau::TableType>(instanceGlobal.value()))
            if (auto newFunction = ttv->props.find("new"); newFunction != ttv->props.end())
            {
                if (metadata.has_value() && !metadata->CREATABLE_INSTANCES.empty() && Luau::get<Luau::FunctionType>(newFunction->second.type()))
                {
                    Luau::attachTag(newFunction->second.type(), "CreatableInstances");
                    Luau::attachMagicFunction(
                        newFunction->second.type(), createMagicFunctionTypeLookup(metadata->CREATABLE_INSTANCES, "Invalid class name"));
                }
                else
                {
                    // TODO: snip old code and move metadata check above
                    if (auto itv = Luau::get<Luau::IntersectionType>(newFunction->second.type()))
                        for (auto& part : itv->parts)
                            if (auto ftv = Luau::get<Luau::FunctionType>(part))
                                if (auto it = Luau::begin(ftv->argTypes); it != Luau::end(ftv->argTypes))
                                    if (Luau::isPrim(*it, Luau::PrimitiveType::String))
                                        Luau::attachMagicFunction(part, magicFunctionInstanceNew);
                }
            }

    // Attach onto `game:GetService()`
    if (auto serviceProviderType = globals.globalScope->lookupType("ServiceProvider"))
        if (auto* ctv = Luau::getMutable<Luau::ClassType>(serviceProviderType->type))
        {
            if (metadata.has_value() && !metadata->CREATABLE_INSTANCES.empty() && Luau::get<Luau::FunctionType>(ctv->props["GetService"].type()))
            {
                Luau::attachTag(ctv->props["GetService"].type(), "Services");
                Luau::attachMagicFunction(ctv->props["GetService"].type(), createMagicFunctionTypeLookup(metadata->SERVICES, "Invalid service name"));
            }
            else
            {
                // TODO: snip old code and move metadata check above
                // Mark `game:GetService()` with a tag so we can prioritise services when autocompleting
                // :GetService is an intersection of function types, so we assign a tag on the first intersection
                if (auto* itv = Luau::getMutable<Luau::IntersectionType>(ctv->props["GetService"].type()); itv && !itv->parts.empty())
                    Luau::attachTag(itv->parts[0], "PrioritiseCommonServices");
            }
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
                    Luau::attachMagicFunction(ctv->props["IsA"].type(), types::magicFunctionEnumItemIsA);
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

    return loadResult;
}

using NameOrExpr = std::variant<std::string, Luau::AstExpr*>;

// Converts an FTV and function call to a nice string
// In the format "function NAME(args): ret"
std::string toStringNamedFunction(const Luau::ModulePtr& module, const Luau::FunctionType* ftv, const NameOrExpr nameOrFuncExpr,
    std::optional<Luau::ScopePtr> scope, const ToStringNamedFunctionOpts& stringOpts)
{
    Luau::ToStringOptions opts;
    opts.functionTypeArguments = true;
    opts.hideNamedFunctionTypeParameters = false;
    opts.hideTableKind = stringOpts.hideTableKind;
    opts.useLineBreaks = stringOpts.multiline;
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

    // See if it's just in the form `func(args)`
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
        // In the form (expr)(args), which implies that it's probably a IIFE
        return "function" + functionString;
    }

    // See if the name belongs to a ClassType
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

std::string toStringReturnType(Luau::TypePackId retTypes, Luau::ToStringOptions options)
{
    return toStringReturnTypeDetailed(retTypes, std::move(options)).name;
}

Luau::ToStringResult toStringReturnTypeDetailed(Luau::TypePackId retTypes, Luau::ToStringOptions options)
{
    size_t retSize = Luau::size(retTypes);
    bool hasTail = !Luau::finite(retTypes);
    bool wrap = Luau::get<Luau::TypePack>(Luau::follow(retTypes)) && (hasTail ? retSize != 0 : retSize != 1);

    auto result = Luau::toStringDetailed(retTypes, options);
    if (wrap)
        result.name = "(" + result.name + ")";
    return result;
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

struct FindNodeType : public Luau::AstVisitor
{
    Luau::Position pos;
    Luau::Position documentEnd;
    Luau::AstNode* best = nullptr;
    bool closed = false;

    explicit FindNodeType(Luau::Position pos, Luau::Position documentEnd, bool closed)
        : pos(pos)
        , documentEnd(documentEnd)
        , closed(closed)
    {
    }

    bool isCloserMatch(Luau::Location& newLocation) const
    {
        return (closed ? newLocation.containsClosed(pos) : newLocation.contains(pos)) && (!best || best->location.encloses(newLocation));
    }

    bool visit(Luau::AstNode* node) override
    {
        if (isCloserMatch(node->location))
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

    bool visit(class Luau::AstType* node) override
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

    FindNodeType findNode{pos, end, /* closed: */ false};
    findNode.visit(source.root);
    return findNode.best;
}

Luau::AstNode* findNodeOrTypeAtPositionClosed(const Luau::SourceModule& source, Luau::Position pos)
{
    const Luau::Position end = source.root->location.end;
    if (pos < source.root->location.begin)
        return source.root;

    if (pos > end)
        pos = end;

    FindNodeType findNode{pos, end, /* closed: */ true};
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
    if (auto ctv = Luau::get<Luau::ClassType>(parentType))
    {
        if (auto prop = Luau::lookupClassProp(ctv, name))
            return *prop;
    }
    else if (auto tbl = Luau::get<Luau::TableType>(parentType))
    {
        if (tbl->props.find(name) != tbl->props.end())
        {
            return tbl->props.at(name);
        }
    }
    else if (auto mt = Luau::get<Luau::MetatableType>(parentType))
    {
        if (auto mtable = Luau::get<Luau::TableType>(Luau::follow(mt->metatable)))
        {
            auto indexIt = mtable->props.find("__index");
            if (indexIt != mtable->props.end())
            {
                Luau::TypeId followed = Luau::follow(indexIt->second.type());
                if ((Luau::get<Luau::TableType>(followed) || Luau::get<Luau::MetatableType>(followed)) && followed != parentType) // ensure acyclic
                {
                    return lookupProp(followed, name);
                }
                else if (Luau::get<Luau::FunctionType>(followed))
                {
                    // TODO: can we handle an index function...?
                    return std::nullopt;
                }
            }
        }

        if (auto mtBaseTable = Luau::get<Luau::TableType>(Luau::follow(mt->table)))
        {
            if (mtBaseTable->props.find(name) != mtBaseTable->props.end())
            {
                return mtBaseTable->props.at(name);
            }
        }
    }
    // else if (auto i = get<Luau::IntersectionType>(parentType))
    // {
    //     for (Luau::TypeId ty : i->parts)
    //     {
    //         // TODO: find the corresponding ty
    //     }
    // }
    // else if (auto u = get<Luau::UnionType>(parentType))
    // {
    //     // Find the corresponding ty
    // }
    return std::nullopt;
}

std::optional<Luau::ModuleName> lookupImportedModule(const Luau::Scope& deepScope, const Luau::Name& name)
{
    const Luau::Scope* scope = &deepScope;
    while (true)
    {
        auto it = scope->importedModules.find(name);
        if (it != scope->importedModules.end())
            return it->second;

        if (scope->parent)
            scope = scope->parent.get();
        else
            return std::nullopt;
    }
}

bool types::isMetamethod(const Luau::Name& name)
{
    return name == "__index" || name == "__newindex" || name == "__call" || name == "__concat" || name == "__unm" || name == "__add" ||
           name == "__sub" || name == "__mul" || name == "__div" || name == "__mod" || name == "__pow" || name == "__tostring" ||
           name == "__metatable" || name == "__eq" || name == "__lt" || name == "__le" || name == "__mode" || name == "__iter" || name == "__len";
}

lsp::Position toUTF16(const TextDocument* textDocument, const Luau::Position& position)
{
    if (textDocument)
        return textDocument->convertPosition(position);
    else
        return lsp::Position{static_cast<size_t>(position.line), static_cast<size_t>(position.column)};
}

lsp::Diagnostic createTypeErrorDiagnostic(const Luau::TypeError& error, Luau::FileResolver* fileResolver, const TextDocument* textDocument)
{
    std::string message;
    if (const auto* syntaxError = Luau::get_if<Luau::SyntaxError>(&error.data))
        message = "SyntaxError: " + syntaxError->message;
    else
        message = "TypeError: " + Luau::toString(error, Luau::TypeErrorToStringOptions{fileResolver});

    lsp::Diagnostic diagnostic;
    diagnostic.source = "Luau";
    diagnostic.code = error.code();
    diagnostic.message = message;
    diagnostic.severity = lsp::DiagnosticSeverity::Error;
    diagnostic.range = {toUTF16(textDocument, error.location.begin), toUTF16(textDocument, error.location.end)};
    diagnostic.codeDescription = {Uri::parse("https://luau-lang.org/typecheck")};
    return diagnostic;
}

lsp::Diagnostic createLintDiagnostic(const Luau::LintWarning& lint, const TextDocument* textDocument)
{
    std::string lintName = Luau::LintWarning::getName(lint.code);

    lsp::Diagnostic diagnostic;
    diagnostic.source = "Luau";
    diagnostic.code = lint.code;
    diagnostic.message = lintName + ": " + lint.text;
    diagnostic.severity = lsp::DiagnosticSeverity::Warning; // Configuration can convert this to an error
    diagnostic.range = {toUTF16(textDocument, lint.location.begin), toUTF16(textDocument, lint.location.end)};
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

lsp::Diagnostic createParseErrorDiagnostic(const Luau::ParseError& error, const TextDocument* textDocument)
{
    lsp::Diagnostic diagnostic;
    diagnostic.source = "Luau";
    diagnostic.code = "SyntaxError";
    diagnostic.message = "SyntaxError: " + error.getMessage();
    diagnostic.severity = lsp::DiagnosticSeverity::Error;
    diagnostic.range = {toUTF16(textDocument, error.getLocation().begin), toUTF16(textDocument, error.getLocation().end)};
    diagnostic.codeDescription = {Uri::parse("https://luau-lang.org/syntax")};
    return diagnostic;
}

// Based on upstream, except we use containsClosed
struct FindExprOrLocalClosed : public Luau::AstVisitor
{
    Luau::Position pos;
    Luau::ExprOrLocal result;

    explicit FindExprOrLocalClosed(Luau::Position pos)
        : pos(pos)
    {
    }

    // We want to find the result with the smallest location range.
    bool isCloserMatch(Luau::Location newLocation)
    {
        auto current = result.getLocation();
        return newLocation.containsClosed(pos) && (!current || current->encloses(newLocation));
    }

    bool visit(Luau::AstStatBlock* block) override
    {
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

    bool visit(Luau::AstExpr* expr) override
    {
        if (isCloserMatch(expr->location))
        {
            result.setExpr(expr);
            return true;
        }
        return false;
    }

    bool visitLocal(Luau::AstLocal* local)
    {
        if (isCloserMatch(local->location))
        {
            result.setLocal(local);
            return true;
        }
        return false;
    }

    bool visit(Luau::AstStatLocalFunction* function) override
    {
        visitLocal(function->name);
        return true;
    }

    bool visit(Luau::AstStatLocal* al) override
    {
        for (size_t i = 0; i < al->vars.size; ++i)
        {
            visitLocal(al->vars.data[i]);
        }
        return true;
    }

    bool visit(Luau::AstExprFunction* fn) override
    {
        for (size_t i = 0; i < fn->args.size; ++i)
        {
            visitLocal(fn->args.data[i]);
        }
        return visit((Luau::AstExpr*)fn);
    }

    bool visit(Luau::AstStatFor* forStat) override
    {
        visitLocal(forStat->var);
        return true;
    }

    bool visit(Luau::AstStatForIn* forIn) override
    {
        for (Luau::AstLocal* var : forIn->vars)
        {
            visitLocal(var);
        }
        return true;
    }
};

Luau::ExprOrLocal findExprOrLocalAtPositionClosed(const Luau::SourceModule& source, Luau::Position pos)
{
    FindExprOrLocalClosed findVisitor{pos};
    findVisitor.visit(source.root);
    return findVisitor.result;
}

struct FindSymbolReferences : public Luau::AstVisitor
{
    Luau::Symbol symbol;
    std::vector<Luau::Location> result{};

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

    bool visitLocal(Luau::AstLocal* local) const
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

    bool visit(Luau::AstExprLocal* local) override
    {
        if (visitLocal(local->local))
        {
            result.push_back(local->location);
        }
        return true;
    }

    bool visit(Luau::AstExprFunction* fn) override
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

    bool visit(Luau::AstStatFor* forStat) override
    {
        if (visitLocal(forStat->var))
        {
            result.push_back(forStat->var->location);
        }
        return true;
    }

    bool visit(Luau::AstStatForIn* forIn) override
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

struct FindTypeReferences : public Luau::AstVisitor
{
    Luau::Name typeName;
    std::optional<const Luau::Name> prefix;
    std::vector<Luau::Location> result{};

    explicit FindTypeReferences(Luau::Name typeName, std::optional<const Luau::Name> prefix)
        : typeName(std::move(typeName))
        , prefix(std::move(prefix))
    {
    }

    bool visit(class Luau::AstType* node) override
    {
        return true;
    }

    bool visit(class Luau::AstTypeReference* node) override
    {
        if (node->name.value == typeName && ((!prefix && !node->prefix) || (prefix && node->prefix && node->prefix->value == prefix.value())))
            result.push_back(node->nameLocation);

        return true;
    }
};

std::vector<Luau::Location> findTypeReferences(const Luau::SourceModule& source, const Luau::Name& typeName, std::optional<const Luau::Name> prefix)
{
    FindTypeReferences finder(typeName, std::move(prefix));
    source.root->visit(&finder);
    return std::move(finder.result);
}

std::optional<Luau::Location> getLocation(Luau::TypeId type)
{
    type = follow(type);

    if (auto ftv = Luau::get<Luau::FunctionType>(type))
    {
        if (ftv->definition)
            return ftv->definition->originalNameLocation;
    }

    return std::nullopt;
}

bool isGetService(const Luau::AstExpr* expr)
{
    if (auto call = expr->as<Luau::AstExprCall>())
        if (auto index = call->func->as<Luau::AstExprIndexName>())
            if (index->index == "GetService")
                if (auto name = index->expr->as<Luau::AstExprGlobal>())
                    if (name->name == "game")
                        return true;

    return false;
}

bool isRequire(const Luau::AstExpr* expr)
{
    if (auto call = expr->as<Luau::AstExprCall>())
    {
        if (auto funcAsGlobal = call->func->as<Luau::AstExprGlobal>(); funcAsGlobal && funcAsGlobal->name == "require")
            return true;
    }
    else if (auto assertion = expr->as<Luau::AstExprTypeAssertion>())
    {
        return isRequire(assertion->expr);
    }

    return false;
}