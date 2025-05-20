#include "Luau/TypeFwd.h"
#include "Platform/RobloxPlatform.hpp"

#include "LSP/Workspace.hpp"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/ConstraintSolver.h"
#include "Luau/TimeTrace.h"

LUAU_FASTFLAG(LuauSolverV2)

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

static Luau::TypeId getSourcemapType(const Luau::GlobalTypes& globals, Luau::TypeArena& arena, const SourceNodePtr& node);

struct MagicChildLookup final : Luau::MagicFunction
{
    const Luau::GlobalTypes& globals;
    Luau::TypeArena& arena;
    SourceNodePtr node;
    bool supportsRecursiveParameter;
    bool supportsTimeoutParameter;

    MagicChildLookup(const Luau::GlobalTypes& globals, Luau::TypeArena& arena, SourceNodePtr node, bool supportsRecursiveParameter = false,
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

static void attachChildLookupFunction(const Luau::GlobalTypes& globals, Luau::TypeArena& arena, const SourceNodePtr& node, Luau::TypeId lookupFuncTy,
    bool supportsRecursiveParameter = false, bool supportsTimeoutParameter = false)
{
    Luau::attachMagicFunction(
        lookupFuncTy, std::make_shared<MagicChildLookup>(globals, arena, node, supportsRecursiveParameter, supportsTimeoutParameter));
    Luau::attachTag(lookupFuncTy, kSourcemapGeneratedTag);
    Luau::attachTag(lookupFuncTy, "Children");
}

static void injectChildrenLookupFunctions(
    const Luau::GlobalTypes& globals, Luau::TypeArena& arena, Luau::ExternType* ctv, const Luau::TypeId& ty, const SourceNodePtr& node)
{
    if (auto instanceType = getTypeIdForClass(globals.globalScope, "Instance"))
    {
        auto optionalInstanceType = Luau::makeOption(globals.builtinTypes, arena, *instanceType);
        auto findFirstChildFunction = Luau::makeFunction(arena, ty,
            {globals.builtinTypes->stringType, Luau::makeOption(globals.builtinTypes, arena, globals.builtinTypes->booleanType)},
            {"name", "recursive"}, {optionalInstanceType});

        Luau::TypeId waitForChildFunction;
        if (FFlag::LuauSolverV2)
            waitForChildFunction = Luau::makeFunction(
                arena, ty, {globals.builtinTypes->stringType, globals.builtinTypes->optionalNumberType}, {"name"}, {*instanceType});
        else
            waitForChildFunction = Luau::makeFunction(arena, ty, {globals.builtinTypes->stringType}, {"name"}, {*instanceType});
        auto waitForChildWithTimeoutFunction = Luau::makeFunction(
            arena, ty, {globals.builtinTypes->stringType, globals.builtinTypes->numberType}, {"name", "timeout"}, {optionalInstanceType});

        attachChildLookupFunction(globals, arena, node, findFirstChildFunction, /* supportsRecursiveParameter= */ true);
        ctv->props["FindFirstChild"] = Luau::makeProperty(findFirstChildFunction, "@roblox/globaltype/Instance.FindFirstChild");

        if (FFlag::LuauSolverV2)
        {
            attachChildLookupFunction(
                globals, arena, node, waitForChildFunction, /* supportsRecursiveParameter= */ false, /*supportsTimeoutParameter= */ true);
            ctv->props["WaitForChild"] = Luau::makeProperty(waitForChildFunction, "@roblox/globaltype/Instance.WaitForChild");
        }
        else
        {
            attachChildLookupFunction(globals, arena, node, waitForChildFunction);
            attachChildLookupFunction(globals, arena, node, waitForChildWithTimeoutFunction);
            ctv->props["WaitForChild"] = Luau::makeProperty(
                Luau::makeIntersection(arena, {waitForChildFunction, waitForChildWithTimeoutFunction}), "@roblox/globaltype/Instance.WaitForChild");
        }
    }
}

struct MagicFindFirstAncestor final : Luau::MagicFunction
{
    const Luau::GlobalTypes& globals;
    Luau::TypeArena& arena;
    const SourceNodePtr& node;

    MagicFindFirstAncestor(const Luau::GlobalTypes& globals, Luau::TypeArena& arena, const SourceNodePtr& node)
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
                if (auto parentNode = node->parent.lock())
                    ctv->props["Parent"] = Luau::makeProperty(getSourcemapType(globals, arena, parentNode));

                // Add children as properties
                for (const auto& child : node->children)
                    ctv->props[child->name] = ctv->props[child->name] = Luau::Property{
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

void addChildrenToCTV(const Luau::GlobalTypes& globals, Luau::TypeArena& arena, const Luau::TypeId& ty, const SourceNodePtr& node)
{
    if (auto* ctv = Luau::getMutable<Luau::ExternType>(ty))
    {
        // Clear out all the old registered children
        for (auto it = ctv->props.begin(); it != ctv->props.end();)
        {
            if (hasTag(it->second, kSourcemapGeneratedTag))
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
                /* tags */ {kSourcemapGeneratedTag},
                /* documentationSymbol*/ std::nullopt,
            };
        }

        // Add children lookup function
        injectChildrenLookupFunctions(globals, arena, ctv, ty, node);
    }
}

bool RobloxPlatform::updateSourceMapFromContents(const std::string& sourceMapContents)
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::updateSourceMapFromContents", "LSP");
    workspaceFolder->client->sendTrace("Sourcemap file read successfully");

    workspaceFolder->frontend.clear();
    updateSourceNodeMap(sourceMapContents);

    workspaceFolder->client->sendTrace("Loaded sourcemap nodes");

    // Recreate instance types
    instanceTypes.clear(); // NOTE: used across BOTH instances of handleSourcemapUpdate, don't clear in between!
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

    workspaceFolder->client->sendTrace("Updating sourcemap contents COMPLETED");

    if (expressiveTypes)
    {
        workspaceFolder->client->sendTrace("Refreshing diagnostics from sourcemap update as strictDatamodelTypes is enabled");
        workspaceFolder->recomputeDiagnostics(config);
    }

    return true;
}

bool RobloxPlatform::updateSourceMap()
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::updateSourceMap", "LSP");
    auto config = workspaceFolder->client->getConfiguration(workspaceFolder->rootUri);
    std::string sourcemapFileName = config.sourcemap.sourcemapFile;

    auto sourcemapPath = workspaceFolder->rootUri.fsPath() / sourcemapFileName;
    workspaceFolder->client->sendTrace("Updating sourcemap contents from " + sourcemapPath.generic_string());

    // Read in the sourcemap
    // TODO: we assume a sourcemap file in the workspace root
    if (auto sourceMapContents = readFile(sourcemapPath))
    {
        return updateSourceMapFromContents(sourceMapContents.value());
    }
    else
    {
        workspaceFolder->client->sendTrace("Sourcemap file failed to read");
        return false;
    }
}

void RobloxPlatform::writePathsToMap(const SourceNodePtr& node, const std::string& base)
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::writePathsToMap", "LSP");
    node->virtualPath = base;
    virtualPathsToSourceNodes[base] = node;

    if (auto realPath = getRealPathFromSourceNode(node))
    {
        realPathsToSourceNodes[realPath->generic_string()] = node;


    }

#ifndef NEVERMORE_STRING_REQUIRE
    for (auto& child : node->children)
    {
        child->parent = node;
        writePathsToMap(child, base + "/" + child->name);
    }
#else
    std::set<std::filesystem::path> scriptChildFilePaths;
    bool hasLoader = false;

    for (auto& child : node->children)
    {
        child->parent = node;
        writePathsToMap(child, base + "/" + child->name);

        if (child->isScript())
        {
            if (child->name == "loader")
            {
                hasLoader = true;
            }

            if (child->className == "ModuleScript")
            {
                this->moduleNameToSourceNode.insert({child->name, child});
            }

            for (std::filesystem::path& filePath : child->filePaths)
            {
                scriptChildFilePaths.insert(filePath.parent_path());
            }
        }
    }

    if (!scriptChildFilePaths.empty() && !hasLoader)
    {
        std::filesystem::path realPath = *(scriptChildFilePaths.begin());

        // Create a virtual source_node for nevermore loader
        std::shared_ptr<struct SourceNode> source_node = std::make_shared<SourceNode>();
        source_node->parent = node;
        source_node->name = "loader";
        source_node->className = "ModuleScript";
        source_node->isVirtualNevermoreLoader = true;
        node->children.push_back(source_node);

        writePathsToMap(source_node, base + "/" + source_node->name);
    }
#endif

}

void RobloxPlatform::updateSourceNodeMap(const std::string& sourceMapContents)
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::updateSourceNodeMap", "LSP");
    realPathsToSourceNodes.clear();
    virtualPathsToSourceNodes.clear();

#ifdef NEVERMORE_STRING_REQUIRE
    moduleNameToSourceNode.clear();
#endif

    try
    {
        auto j = json::parse(sourceMapContents);
        rootSourceNode = std::make_shared<SourceNode>(j.get<SourceNode>());

        // Write paths
        std::string base = rootSourceNode->className == "DataModel" ? "game" : "ProjectRoot";
        writePathsToMap(rootSourceNode, base);
    }
    catch (const std::exception& e)
    {
        // TODO: log message?
        std::cerr << e.what() << '\n';
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

    // Mutate with plugin info
    if (pluginInfo)
    {
        if (rootSourceNode->className == "DataModel")
        {
            mutateSourceNodeWithPluginInfo(*rootSourceNode, pluginInfo);
        }
        else
        {
            std::cerr << "Attempted to update plugin information for a non-DM instance" << '\n';
        }
    }

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

#ifdef NEVERMORE_STRING_REQUIRE
        if (auto node = isVirtualPath(name) ? getSourceNodeFromVirtualPath(name) : getSourceNodeFromRealPath(name))
            scope->bindings[Luau::AstName{"StringRequire"}] = Luau::Binding{getStringRequireType(globals, instanceTypes, node.value())};
#endif

        if (expressiveTypes || forAutocomplete)
            if (auto node = isVirtualPath(name) ? getSourceNodeFromVirtualPath(name) : getSourceNodeFromRealPath(name))
                scope->bindings[Luau::AstName("script")] = Luau::Binding{getSourcemapType(globals, instanceTypes, node.value())};
    };
}

std::optional<SourceNodePtr> RobloxPlatform::getSourceNodeFromVirtualPath(const Luau::ModuleName& name) const
{
    if (virtualPathsToSourceNodes.find(name) == virtualPathsToSourceNodes.end())
        return std::nullopt;
    return virtualPathsToSourceNodes.at(name);
}

std::optional<SourceNodePtr> RobloxPlatform::getSourceNodeFromRealPath(const std::string& name) const
{
    std::error_code ec;
    auto canonicalName = std::filesystem::weakly_canonical(name, ec);
    if (ec.value() != 0)
        canonicalName = name;
    // URI-ify the file path so that its normalised (in particular, the drive letter)
    canonicalName = Uri::file(canonicalName).fsPath();
    auto strName = canonicalName.generic_string();
    if (realPathsToSourceNodes.find(strName) == realPathsToSourceNodes.end())
        return std::nullopt;
    return realPathsToSourceNodes.at(strName);
}

Luau::ModuleName RobloxPlatform::getVirtualPathFromSourceNode(const SourceNodePtr& sourceNode)
{
    return sourceNode->virtualPath;
}

std::optional<std::filesystem::path> RobloxPlatform::getRealPathFromSourceNode(const SourceNodePtr& sourceNode) const
{
    // NOTE: this filepath is generated by the sourcemap, which is relative to the cwd where the sourcemap
    // command was run from. Hence, we concatenate it to the end of the workspace path, and normalise the result
    // TODO: make sure this is correct once we make sourcemap.json generic
    if (auto filePath = sourceNode->getScriptFilePath())
    {
        std::error_code ec;
        auto canonicalName = std::filesystem::weakly_canonical(fileResolver->rootUri.fsPath() / *filePath, ec);
        if (ec.value() != 0)
            canonicalName = *filePath;
        // URI-ify the file path so that its normalised (in particular, the drive letter)
        return Uri::file(canonicalName).fsPath();
    }

    return std::nullopt;
}
