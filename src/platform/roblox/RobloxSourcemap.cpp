#include "Platform/RobloxPlatform.hpp"

#include "Luau/BuiltinDefinitions.h"

static void mutateSourceNodeWithPluginInfo(SourceNode& sourceNode, const PluginNodePtr& pluginInstance)
{
    // We currently perform purely additive changes where we add in new children
    for (const auto& dmChild : pluginInstance->children)
    {
        if (auto existingChildNode = sourceNode.findChild(dmChild->name))
        {
            mutateSourceNodeWithPluginInfo(*existingChildNode.value(), dmChild);
        }
        else
        {
            SourceNode childNode;
            childNode.name = dmChild->name;
            childNode.className = dmChild->className;
            mutateSourceNodeWithPluginInfo(childNode, dmChild);

            sourceNode.children.push_back(std::make_shared<SourceNode>(childNode));
        }
    }
}

// Retrieves the corresponding Luau type for a Sourcemap node
// If it does not yet exist, the type is produced
static Luau::TypeId getSourcemapType(const Luau::GlobalTypes& globals, Luau::TypeArena& arena, const SourceNodePtr& node)
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
            Luau::TypeId baseTypeId = types::getTypeIdForClass(globals.globalScope, node->className).value_or(nullptr);
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
            std::string typeName = types::getTypeName(baseTypeId).value_or(node->name);
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
                if (auto instanceType = types::getTypeIdForClass(globals.globalScope, "Instance"))
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

static void addChildrenToCTV(const Luau::GlobalTypes& globals, Luau::TypeArena& arena, const Luau::TypeId& ty, const SourceNodePtr& node)
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
void RobloxPlatform::handleSourcemapUpdate(
    Luau::Frontend& frontend, const Luau::GlobalTypes& globals, const WorkspaceFileResolver& fileResolver, bool expressiveTypes)
{
    if (!fileResolver.rootSourceNode)
        return;

    // Mutate with plugin info
    if (pluginInfo)
    {
        if (fileResolver.rootSourceNode->className == "DataModel")
        {
            mutateSourceNodeWithPluginInfo(*fileResolver.rootSourceNode, pluginInfo);
        }
        else
        {
            std::cerr << "Attempted to update plugin information for a non-DM instance" << '\n';
        }
    }

    // Recreate instance types
    instanceTypes.clear();

    // Create a type for the root source node
    getSourcemapType(globals, instanceTypes, fileResolver.rootSourceNode);

    // Modify sourcemap types
    if (fileResolver.rootSourceNode->className == "DataModel")
    {
        // Mutate DataModel with its children
        if (auto dataModelType = globals.globalScope->lookupType("DataModel"))
            addChildrenToCTV(globals, instanceTypes, dataModelType->type, fileResolver.rootSourceNode);

        // Mutate globally-registered Services to include children information (so it's available through :GetService)
        for (const auto& service : fileResolver.rootSourceNode->children)
        {
            auto serviceName = service->className; // We know it must be a service of the same class name
            if (auto serviceType = globals.globalScope->lookupType(serviceName))
                addChildrenToCTV(globals, instanceTypes, serviceType->type, service);
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
                        addChildrenToCTV(globals, instanceTypes, playerGuiType->type, *starterGui);
                    ctv->props["PlayerGui"] = Luau::makeProperty(playerGuiType->type);
                }

                // Player.StarterGear should contain StarterPack instances
                if (auto starterGearType = globals.globalScope->lookupType("StarterGear"))
                {
                    if (auto starterPack = fileResolver.rootSourceNode->findChild("StarterPack"))
                        addChildrenToCTV(globals, instanceTypes, starterGearType->type, *starterPack);

                    ctv->props["StarterGear"] = Luau::makeProperty(starterGearType->type);
                }

                // Player.PlayerScripts should contain StarterPlayerScripts instances
                if (auto playerScriptsType = globals.globalScope->lookupType("PlayerScripts"))
                {
                    if (auto starterPlayer = fileResolver.rootSourceNode->findChild("StarterPlayer"))
                    {
                        if (auto starterPlayerScripts = starterPlayer.value()->findChild("StarterPlayerScripts"))
                        {
                            addChildrenToCTV(globals, instanceTypes, playerScriptsType->type, *starterPlayerScripts);
                        }
                    }
                    ctv->props["PlayerScripts"] = Luau::makeProperty(playerScriptsType->type);
                }
            }
        }
    }

    // Prepare module scope so that we can dynamically reassign the type of "script" to retrieve instance info
    frontend.prepareModuleScope = [this, &frontend, &fileResolver, expressiveTypes](
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
                scope->bindings[Luau::AstName("script")] = Luau::Binding{getSourcemapType(globals, instanceTypes, node.value())};
    };
}
