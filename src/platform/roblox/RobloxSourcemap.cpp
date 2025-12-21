#include "Luau/TypeFwd.h"
#include "Platform/RobloxPlatform.hpp"

#include "LSP/Workspace.hpp"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/ConstraintSolver.h"
#include "Luau/TimeTrace.h"
#include "LuauFileUtils.hpp"

LUAU_FASTFLAG(LuauSolverV2)

static std::optional<Luau::TypeId> getTypeIdForClass(const Luau::ScopePtr& globalScope, std::optional<std::string> className)
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

static Luau::TypeId getSourcemapType(const Luau::GlobalTypes& globals, Luau::TypeArena& arena, const SourceNode* node);

struct MagicChildLookup final : Luau::MagicFunction
{
    const Luau::GlobalTypes& globals;
    Luau::TypeArena& arena;
    const SourceNode* node;
    bool supportsRecursiveParameter;
    bool supportsTimeoutParameter;

    MagicChildLookup(const Luau::GlobalTypes& globals, Luau::TypeArena& arena, const SourceNode* node, bool supportsRecursiveParameter = false,
        bool supportsTimeoutParameter = false)
        : globals(globals)
        , arena(arena)
        , node(std::move(node))
        , supportsRecursiveParameter(supportsRecursiveParameter)
        , supportsTimeoutParameter(supportsTimeoutParameter)
    {
    }

    std::optional<Luau::WithPredicate<Luau::TypePackId>> handleOldSolver(Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope,
        const Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId> withPredicate) override;
    bool infer(const Luau::MagicFunctionCallContext& context) override;
};

std::optional<Luau::WithPredicate<Luau::TypePackId>> MagicChildLookup::handleOldSolver(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId>)
{
    if (expr.args.size < 1)
        return std::nullopt;

    auto str = expr.args.data[0]->as<Luau::AstExprConstantString>();
    if (!str)
        return std::nullopt;

    bool recursive = false;
    if (supportsRecursiveParameter && expr.args.size >= 2)
        if (auto recursiveParameter = expr.args.data[1]->as<Luau::AstExprConstantBool>())
            recursive = recursiveParameter->value;

    auto childName = std::string(str->value.data, str->value.size);
    auto child = recursive ? node->findDescendant(childName) : node->findChild(childName);
    if (child)
        return Luau::WithPredicate<Luau::TypePackId>{arena.addTypePack({getSourcemapType(globals, arena, *child)})};

    return std::nullopt;
}

bool MagicChildLookup::infer(const Luau::MagicFunctionCallContext& context)
{
    if (context.callSite->args.size < 1)
        return false;

    auto str = context.callSite->args.data[0]->as<Luau::AstExprConstantString>();
    if (!str)
        return false;

    bool recursive = false;
    bool timeoutEnabled = false;
    if (context.callSite->args.size >= 2)
    {
        if (auto recursiveParameter = context.callSite->args.data[1]->as<Luau::AstExprConstantBool>();
            recursiveParameter && supportsRecursiveParameter)
            recursive = recursiveParameter->value;
        if (supportsTimeoutParameter)
            timeoutEnabled = true;
    }

    auto childName = std::string(str->value.data, str->value.size);
    auto child = recursive ? node->findDescendant(childName) : node->findChild(childName);
    if (child)
    {
        asMutable(context.result)->ty.emplace<Luau::BoundTypePack>(context.solver->arena->addTypePack({getSourcemapType(globals, arena, *child)}));
        return true;
    }

    if (timeoutEnabled)
    {
        auto optionalInstanceType = Luau::makeOption(globals.builtinTypes, arena, getTypeIdForClass(globals.globalScope, "Instance").value());
        asMutable(context.result)->ty.emplace<Luau::BoundTypePack>(context.solver->arena->addTypePack({optionalInstanceType}));
        return true;
    }

    return false;
}

static void attachChildLookupFunction(const Luau::GlobalTypes& globals, Luau::TypeArena& arena, const SourceNode* node, Luau::TypeId lookupFuncTy,
    bool supportsRecursiveParameter = false, bool supportsTimeoutParameter = false)
{
    Luau::attachMagicFunction(
        lookupFuncTy, std::make_shared<MagicChildLookup>(globals, arena, node, supportsRecursiveParameter, supportsTimeoutParameter));
    Luau::attachTag(lookupFuncTy, kSourcemapGeneratedTag);
    Luau::attachTag(lookupFuncTy, "Children");
}

static void injectChildrenLookupFunctions(
    const Luau::GlobalTypes& globals, Luau::TypeArena& arena, Luau::ExternType* ctv, const Luau::TypeId& ty, const SourceNode* node)
{
    if (auto instanceType = getTypeIdForClass(globals.globalScope, "Instance"))
    {
        auto optionalInstanceType = Luau::makeOption(globals.builtinTypes, arena, *instanceType);
        auto findFirstChildFunction = Luau::makeFunction(arena, ty,
            {globals.builtinTypes->stringType, Luau::makeOption(globals.builtinTypes, arena, globals.builtinTypes->booleanType)},
            {"name", "recursive"}, {optionalInstanceType});

        attachChildLookupFunction(globals, arena, node, findFirstChildFunction, /* supportsRecursiveParameter= */ true);
        ctv->props["FindFirstChild"] = Luau::Property{
            /* type */ findFirstChildFunction,
            /* deprecated */ false,
            /* deprecatedSuggestion */ {},
            /* location */ std::nullopt,
            /* tags */ {kSourcemapGeneratedTag},
            "@roblox/globaltype/Instance.FindFirstChild",
        };

        if (FFlag::LuauSolverV2)
        {
            auto waitForChildFunction = Luau::makeFunction(
                arena, ty, {globals.builtinTypes->stringType, globals.builtinTypes->optionalNumberType}, {"name", "timeout"}, {*instanceType});
            attachChildLookupFunction(
                globals, arena, node, waitForChildFunction, /* supportsRecursiveParameter= */ false, /*supportsTimeoutParameter= */ true);
            ctv->props["WaitForChild"] = Luau::Property{
                /* type */ waitForChildFunction,
                /* deprecated */ false,
                /* deprecatedSuggestion */ {},
                /* location */ std::nullopt,
                /* tags */ {kSourcemapGeneratedTag},
                "@roblox/globaltype/Instance.WaitForChild",
            };
        }
        else
        {
            auto waitForChildFunction = Luau::makeFunction(arena, ty, {globals.builtinTypes->stringType}, {"name"}, {*instanceType});
            auto waitForChildWithTimeoutFunction = Luau::makeFunction(
                arena, ty, {globals.builtinTypes->stringType, globals.builtinTypes->numberType}, {"name", "timeout"}, {optionalInstanceType});
            attachChildLookupFunction(globals, arena, node, waitForChildFunction);
            attachChildLookupFunction(globals, arena, node, waitForChildWithTimeoutFunction);
            ctv->props["WaitForChild"] = Luau::Property{
                /* type */ Luau::makeIntersection(arena, {waitForChildFunction, waitForChildWithTimeoutFunction}),
                /* deprecated */ false,
                /* deprecatedSuggestion */ {},
                /* location */ std::nullopt,
                /* tags */ {kSourcemapGeneratedTag},
                "@roblox/globaltype/Instance.WaitForChild",
            };
        }
    }
}

struct MagicFindFirstAncestor final : Luau::MagicFunction
{
    const Luau::GlobalTypes& globals;
    Luau::TypeArena& arena;
    const SourceNode* node;

    MagicFindFirstAncestor(const Luau::GlobalTypes& globals, Luau::TypeArena& arena, const SourceNode* node)
        : globals(globals)
        , arena(arena)
        , node(node)
    {
    }

    std::optional<Luau::WithPredicate<Luau::TypePackId>> handleOldSolver(Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope,
        const Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId> withPredicate) override;
    bool infer(const Luau::MagicFunctionCallContext& context) override;
};

std::optional<Luau::WithPredicate<Luau::TypePackId>> MagicFindFirstAncestor::handleOldSolver(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId> withPredicate)
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
}

bool MagicFindFirstAncestor::infer(const Luau::MagicFunctionCallContext& context)
{
    if (context.callSite->args.size < 1)
        return false;

    auto str = context.callSite->args.data[0]->as<Luau::AstExprConstantString>();
    if (!str)
        return false;

    if (auto ancestor = node->findAncestor(std::string(str->value.data, str->value.size)))
    {
        asMutable(context.result)->ty.emplace<Luau::BoundTypePack>(context.solver->arena->addTypePack({getSourcemapType(globals, arena, *ancestor)}));
        return true;
    }
    return false;
}

// Retrieves the corresponding Luau type for a Sourcemap node
// If it does not yet exist, the type is produced
static Luau::TypeId getSourcemapType(const Luau::GlobalTypes& globals, Luau::TypeArena& arena, const SourceNode* node)
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
            auto* instanceCtv = Luau::get<Luau::ExternType>(instanceTy->type);
            if (!instanceCtv)
            {
                ltv.unwrapped = globals.builtinTypes->anyType;
                return;
            }
            std::optional<Luau::TypeId> instanceMetaIdentity = instanceCtv->metatable;

            // Create the ClassType representing the instance
            std::string typeName = types::getTypeName(baseTypeId).value_or(node->name);
            Luau::ExternType baseInstanceCtv{
                typeName, {}, baseTypeId, instanceMetaIdentity, {}, {}, instanceCtv->definitionModuleName, instanceCtv->definitionLocation};
            auto typeId = arena.addType(std::move(baseInstanceCtv));

            // Attach Parent and Children info
            // Get the mutable version of the type var
            if (auto* ctv = Luau::getMutable<Luau::ExternType>(typeId))
            {
                if (node->parent)
                {
                    if (FFlag::LuauSolverV2)
                        ctv->props["Parent"] = Luau::Property::rw(getSourcemapType(globals, arena, node->parent), instanceTy->type);
                    else
                        ctv->props["Parent"] = Luau::makeProperty(getSourcemapType(globals, arena, node->parent));
                }

                // Add children as properties
                for (const auto& child : node->children)
                    ctv->props[child->name] = Luau::Property{
                        getSourcemapType(globals, arena, child),
                        /* deprecated */ false,
                        /* deprecatedSuggestion */ {},
                        /* location */ std::nullopt,
                        /* tags */ {kSourcemapGeneratedTag},
                        /* documentationSymbol*/ std::nullopt,
                    };

                // Add FindFirstAncestor and FindFirstChild
                if (auto instanceType = getTypeIdForClass(globals.globalScope, "Instance"))
                {
                    auto findFirstAncestorFunction = Luau::makeFunction(
                        arena, typeId, {globals.builtinTypes->stringType}, {"name"}, {Luau::makeOption(globals.builtinTypes, arena, *instanceType)});

                    Luau::attachMagicFunction(findFirstAncestorFunction, std::make_shared<MagicFindFirstAncestor>(globals, arena, node));
                    ctv->props["FindFirstAncestor"] = Luau::makeProperty(findFirstAncestorFunction, "@roblox/globaltype/Instance.FindFirstAncestor");

                    injectChildrenLookupFunctions(globals, arena, ctv, typeId, node);
                }
            }

            ltv.unwrapped = typeId;
            return;
        });
    auto ty = arena.addType(std::move(ltv));
    node->tys.insert_or_assign(&globals, ty);

    return ty;
}

void addChildrenToCTV(const Luau::GlobalTypes& globals, Luau::TypeArena& arena, const Luau::TypeId& ty, const SourceNode* node)
{
    if (auto* ctv = Luau::getMutable<Luau::ExternType>(ty))
    {
        // Extend the props to include the children
        for (const auto& child : node->children)
        {
            ctv->props[child->name] = Luau::Property{
                getSourcemapType(globals, arena, child),
                /* deprecated */ false,
                /* deprecatedSuggestion */ {},
                /* location */ std::nullopt,
                /* tags */ {kSourcemapGeneratedTag},
                /* documentationSymbol*/ std::nullopt,
            };
        }

        // Add children lookup function
        injectChildrenLookupFunctions(globals, arena, ctv, ty, node);
    }
}

static void clearSourcemapGeneratedTypes(Luau::GlobalTypes& globals)
{
    for (const auto& [_, tfun] : globals.globalScope->exportedTypeBindings)
    {
        if (auto* ctv = Luau::getMutable<Luau::ExternType>(tfun.type))
        {
            for (auto it = ctv->props.begin(); it != ctv->props.end();)
            {
                // TODO: will this clear out the generated FindFirstChild/WaitForChild function, is that a problem?
                if (hasTag(it->second, kSourcemapGeneratedTag))
                    it = ctv->props.erase(it);
                else
                    ++it;
            }
        }
    }
}

void RobloxPlatform::clearSourcemapTypes()
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::clearSourcemapTypes", "LSP");
    workspaceFolder->frontend.clear(); // TODO: https://github.com/JohnnyMorganz/luau-lsp/issues/1115
    instanceTypes.clear();             // NOTE: used across BOTH instances of handleSourcemapUpdate, don't clear in between!
    clearSourcemapGeneratedTypes(workspaceFolder->frontend.globals);
    if (!FFlag::LuauSolverV2)
        clearSourcemapGeneratedTypes(workspaceFolder->frontend.globalsForAutocomplete);
}

bool RobloxPlatform::updateSourceMapFromContents(const std::string& sourceMapContents)
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::updateSourceMapFromContents", "LSP");
    workspaceFolder->client->sendTrace("Sourcemap file read successfully");

    updateSourceNodeMap(sourceMapContents);

    workspaceFolder->client->sendTrace("Loaded sourcemap nodes");

    updateSourcemapTypes();

    workspaceFolder->client->sendTrace("Updating sourcemap contents COMPLETED");

    return true;
}

void RobloxPlatform::updateSourcemapTypes()
{
    clearSourcemapTypes();

    auto config = workspaceFolder->client->getConfiguration(workspaceFolder->rootUri);
    bool expressiveTypes = config.diagnostics.strictDatamodelTypes || FFlag::LuauSolverV2;

    // NOTE: expressive types is always enabled for autocomplete, regardless of the setting!
    // We pass the same setting even when we are registering autocomplete globals since
    // the setting impacts what happens to diagnostics (as both calls overwrite frontend.prepareModuleScope)
    workspaceFolder->client->sendTrace("Updating diagnostic types with sourcemap");
    handleSourcemapUpdate(workspaceFolder->frontend, workspaceFolder->frontend.globals, expressiveTypes);
    if (!FFlag::LuauSolverV2)
    {
        workspaceFolder->client->sendTrace("Updating autocomplete types with sourcemap");
        handleSourcemapUpdate(workspaceFolder->frontend, workspaceFolder->frontend.globalsForAutocomplete, expressiveTypes);
    }

    if (expressiveTypes)
    {
        workspaceFolder->client->sendTrace("Refreshing diagnostics from sourcemap update as strictDatamodelTypes is enabled");
        workspaceFolder->recomputeDiagnostics(config);
    }
}

bool RobloxPlatform::updateSourceMap()
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::updateSourceMap", "LSP");
    auto config = workspaceFolder->client->getConfiguration(workspaceFolder->rootUri);
    std::string sourcemapFileName = config.sourcemap.sourcemapFile;

    // TODO: we assume the sourcemap file is in the workspace root
    auto sourcemapPath = workspaceFolder->rootUri.resolvePath(sourcemapFileName);

    if (Luau::FileUtils::exists(sourcemapPath.fsPath()))
    {
        if (auto sourceMapContents = Luau::FileUtils::readFile(sourcemapPath.fsPath()))
        {
            workspaceFolder->client->sendTrace("Updating sourcemap contents from " + sourcemapPath.toString());
            return updateSourceMapFromContents(sourceMapContents.value());
        }
        else
        {
            workspaceFolder->client->sendTrace("Sourcemap file failed to read");
            return false;
        }
    }
    else if (pluginInfo)
    {
        workspaceFolder->client->sendTrace("Creating sourcemap from plugin provided information");
        workspaceFolder->client->sendWindowMessage(
            lsp::MessageType::Info, "Couldn't find " + sourcemapFileName + " for workspace '" + workspaceFolder->name +
                                        "'. Using available datamodel info from companion plugin (require paths may be missing)");
        return updateSourceMapFromContents("{\"name\":\"Default\",\"className\":\"DataModel\",\"children\":[]}");
    }
    else
    {
        workspaceFolder->client->sendTrace("No sourcemap file or plugin information found, cannot update sourcemap");
        return false;
    }
}

void RobloxPlatform::writePathsToMap(SourceNode* node, const std::string& base)
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::writePathsToMap", "LSP");
    node->virtualPath = base;
    virtualPathsToSourceNodes[base] = node;

    if (auto realPath = getRealPathFromSourceNode(node))
    {
        realPathsToSourceNodes.insert_or_assign(*realPath, node);
    }

    for (auto& child : node->children)
    {
        child->parent = node;
        writePathsToMap(child, base + "/" + child->name);
    }
}

void RobloxPlatform::updateSourceNodeMap(const std::string& sourceMapContents)
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::updateSourceNodeMap", "LSP");
    rootSourceNode = nullptr;
    sourceNodeAllocator.clear();
    realPathsToSourceNodes.clear();
    virtualPathsToSourceNodes.clear();

    try
    {
        auto j = json::parse(sourceMapContents);
        rootSourceNode = SourceNode::fromJson(j, sourceNodeAllocator);

        // Mutate with plugin info
        hydrateSourcemapWithPluginInfo();

        // Write paths
        std::string base = rootSourceNode->className == "DataModel" ? "game" : "ProjectRoot";
        writePathsToMap(rootSourceNode, base);
    }
    catch (const std::exception& e)
    {
        // TODO: log message? NOTE: this function can be called from CLI
        std::cerr << "Sourcemap parsing failed, sourcemap is not loaded: " << e.what() << '\n';
        rootSourceNode = nullptr;
        sourceNodeAllocator.clear();
    }
}

// TODO: expressiveTypes is used because of a Luau issue where we can't cast a most specific Instance type (which we create here)
// to another type. For the time being, we therefore make all our DataModel instance types marked as "any".
// Remove this once Luau has improved
void RobloxPlatform::handleSourcemapUpdate(Luau::Frontend& frontend, const Luau::GlobalTypes& globals, bool expressiveTypes)
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::handleSourcemapUpdate", "LSP");
    if (!rootSourceNode)
        return;

    // Create a type for the root source node
    getSourcemapType(globals, instanceTypes, rootSourceNode);

    // Modify sourcemap types
    if (rootSourceNode->className == "DataModel")
    {
        // Mutate DataModel with its children
        if (auto dataModelType = globals.globalScope->lookupType("DataModel"))
            addChildrenToCTV(globals, instanceTypes, dataModelType->type, rootSourceNode);

        // Mutate globally-registered Services to include children information (so it's available through :GetService)
        for (const auto& service : rootSourceNode->children)
        {
            auto serviceName = service->className; // We know it must be a service of the same class name
            if (auto serviceType = globals.globalScope->lookupType(serviceName))
                addChildrenToCTV(globals, instanceTypes, serviceType->type, service);
        }

        // Add containers to player and copy over instances
        // TODO: Player.Character should contain StarterCharacter instances
        if (auto playerType = globals.globalScope->lookupType("Player"))
        {
            if (auto* ctv = Luau::getMutable<Luau::ExternType>(playerType->type))
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
                    if (auto starterGui = rootSourceNode->findChild("StarterGui"))
                        addChildrenToCTV(globals, instanceTypes, playerGuiType->type, *starterGui);
                    ctv->props["PlayerGui"] = Luau::makeProperty(playerGuiType->type);
                }

                // Player.StarterGear should contain StarterPack instances
                if (auto starterGearType = globals.globalScope->lookupType("StarterGear"))
                {
                    if (auto starterPack = rootSourceNode->findChild("StarterPack"))
                        addChildrenToCTV(globals, instanceTypes, starterGearType->type, *starterPack);

                    ctv->props["StarterGear"] = Luau::makeProperty(starterGearType->type);
                }

                // Player.PlayerScripts should contain StarterPlayerScripts instances
                if (auto playerScriptsType = globals.globalScope->lookupType("PlayerScripts"))
                {
                    if (auto starterPlayer = rootSourceNode->findChild("StarterPlayer"))
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
    frontend.prepareModuleScope = [this, &frontend, expressiveTypes](const Luau::ModuleName& name, const Luau::ScopePtr& scope, bool forAutocomplete)
    {
        Luau::GlobalTypes& globals = (forAutocomplete && !FFlag::LuauSolverV2) ? frontend.globalsForAutocomplete : frontend.globals;

        // TODO: we hope to remove these in future!
        if (!expressiveTypes && !forAutocomplete && !FFlag::LuauSolverV2)
        {
            scope->bindings[Luau::AstName("script")] = Luau::Binding{globals.builtinTypes->anyType};
            scope->bindings[Luau::AstName("workspace")] = Luau::Binding{globals.builtinTypes->anyType};
            scope->bindings[Luau::AstName("game")] = Luau::Binding{globals.builtinTypes->anyType};
        }

        if (expressiveTypes || forAutocomplete)
            if (auto node = isVirtualPath(name) ? getSourceNodeFromVirtualPath(name) : getSourceNodeFromRealPath(fileResolver->getUri(name)))
                scope->bindings[Luau::AstName("script")] = Luau::Binding{getSourcemapType(globals, instanceTypes, node.value())};
    };
}

std::optional<const SourceNode*> RobloxPlatform::getSourceNodeFromVirtualPath(const Luau::ModuleName& name) const
{
    if (auto it = virtualPathsToSourceNodes.find(name); it != virtualPathsToSourceNodes.end())
        return it->second;
    return std::nullopt;
}

std::optional<const SourceNode*> RobloxPlatform::getSourceNodeFromRealPath(const Uri& name) const
{
    if (auto it = realPathsToSourceNodes.find(name); it != realPathsToSourceNodes.end())
        return it->second;
    return std::nullopt;
}

Luau::ModuleName RobloxPlatform::getVirtualPathFromSourceNode(const SourceNode* sourceNode)
{
    return sourceNode->virtualPath;
}

std::optional<Uri> RobloxPlatform::getRealPathFromSourceNode(const SourceNode* sourceNode) const
{
    // NOTE: this filepath is generated by the sourcemap, which is relative to the cwd where the sourcemap
    // command was run from. Hence, we concatenate it to the end of the workspace path, and normalise the result
    // TODO: make sure this is correct once we make sourcemap.json generic
    if (auto filePath = sourceNode->getScriptFilePath())
        return fileResolver->rootUri.resolvePath(*filePath);

    return std::nullopt;
}
