#include "Platform/RobloxPlatform.hpp"

#include "LSP/LanguageServer.hpp"
#include "LSP/Utils.hpp"
#include "LSP/ColorProvider.hpp"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/Transpiler.h"
#include "nlohmann/json.hpp"

static constexpr const char* COMMON_SERVICES[] = {
    "Players",
    "ReplicatedStorage",
    "ServerStorage",
    "MessagingService",
    "TeleportService",
    "HttpService",
    "CollectionService",
    "DataStoreService",
    "ContextActionService",
    "UserInputService",
    "Teams",
    "Chat",
    "TextService",
    "TextChatService",
    "GamepadService",
    "VoiceChatService",
};

static constexpr const char* COMMON_INSTANCE_PROPERTIES[] = {
    "Parent",
    "Name",
    // Methods
    "FindFirstChild",
    "IsA",
    "Destroy",
    "GetAttribute",
    "GetChildren",
    "GetDescendants",
    "WaitForChild",
    "Clone",
    "SetAttribute",
};

static constexpr const char* COMMON_SERVICE_PROVIDER_PROPERTIES[] = {
    "GetService",
};

struct RobloxDefinitionsFileMetadata
{
    std::vector<std::string> CREATABLE_INSTANCES{};
    std::vector<std::string> SERVICES{};
};
NLOHMANN_DEFINE_OPTIONAL(RobloxDefinitionsFileMetadata, CREATABLE_INSTANCES, SERVICES)

// Since in Roblox land, debug is extended to introduce more methods, but the api-docs
// mark the package name as `@luau` instead of `@lsp`
static void fixDebugDocumentationSymbol(Luau::TypeId ty, const std::string& libraryName)
{
    auto mutableTy = Luau::asMutable(ty);
    auto newDocumentationSymbol = mutableTy->documentationSymbol.value();
    replace(newDocumentationSymbol, "@lsp", "@luau");
    mutableTy->documentationSymbol = newDocumentationSymbol;

    if (auto* ttv = Luau::getMutable<Luau::TableType>(ty))
    {
        ttv->name = "typeof(" + libraryName + ")";
        for (auto& [name, prop] : ttv->props)
        {
            newDocumentationSymbol = prop.documentationSymbol.value();
            replace(newDocumentationSymbol, "@lsp", "@luau");
            prop.documentationSymbol = newDocumentationSymbol;
        }
    }
}

// Magic function for `Instance:IsA("ClassName")` predicate
static std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionInstanceIsA(Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope,
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
static std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionInstanceClone(Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope,
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
static std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionFindFirstXWhichIsA(Luau::TypeChecker& typeChecker,
    const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, const Luau::WithPredicate<Luau::TypePackId>& withPredicate)
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
static std::optional<Luau::WithPredicate<Luau::TypePackId>> magicFunctionEnumItemIsA(Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope,
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

static lsp::TextEdit createServiceTextEdit(const std::string& name, size_t lineNumber, bool appendNewline = false)
{
    auto range = lsp::Range{{lineNumber, 0}, {lineNumber, 0}};
    auto importText = "local " + name + " = game:GetService(\"" + name + "\")\n";
    if (appendNewline)
        importText += "\n";
    return {range, importText};
}

static lsp::CompletionItem createSuggestService(const std::string& service, size_t lineNumber, bool appendNewline = false)
{
    auto textEdit = createServiceTextEdit(service, lineNumber, appendNewline);

    lsp::CompletionItem item;
    item.label = service;
    item.kind = lsp::CompletionItemKind::Class;
    item.detail = "Auto-import";
    item.documentation = {lsp::MarkupKind::Markdown, codeBlock("luau", textEdit.newText)};
    item.insertText = service;
    item.sortText = SortText::AutoImports;

    item.additionalTextEdits.emplace_back(textEdit);

    return item;
}

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

void RobloxPlatform::handleRegisterDefinitions(Luau::GlobalTypes& globals, std::optional<nlohmann::json> metadata)
{
    // HACK: Mark "debug" using `@luau` symbol instead
    if (auto it = globals.globalScope->bindings.find(Luau::AstName("debug")); it != globals.globalScope->bindings.end())
    {
        auto newDocumentationSymbol = it->second.documentationSymbol.value();
        replace(newDocumentationSymbol, "@lsp", "@luau");
        it->second.documentationSymbol = newDocumentationSymbol;
        fixDebugDocumentationSymbol(it->second.typeId, "debug");
    }

    // HACK: Mark "utf8" using `@luau` symbol instead
    if (auto it = globals.globalScope->bindings.find(Luau::AstName("utf8")); it != globals.globalScope->bindings.end())
    {
        auto newDocumentationSymbol = it->second.documentationSymbol.value();
        replace(newDocumentationSymbol, "@lsp", "@luau");
        it->second.documentationSymbol = newDocumentationSymbol;
        fixDebugDocumentationSymbol(it->second.typeId, "utf8");
    }

    // Extend Instance types
    if (auto instanceType = globals.globalScope->lookupType("Instance"))
    {
        if (auto* ctv = Luau::getMutable<Luau::ClassType>(instanceType->type))
        {
            Luau::attachMagicFunction(ctv->props["IsA"].type(), magicFunctionInstanceIsA);
            Luau::attachMagicFunction(ctv->props["FindFirstChildWhichIsA"].type(), magicFunctionFindFirstXWhichIsA);
            Luau::attachMagicFunction(ctv->props["FindFirstChildOfClass"].type(), magicFunctionFindFirstXWhichIsA);
            Luau::attachMagicFunction(ctv->props["FindFirstAncestorWhichIsA"].type(), magicFunctionFindFirstXWhichIsA);
            Luau::attachMagicFunction(ctv->props["FindFirstAncestorOfClass"].type(), magicFunctionFindFirstXWhichIsA);
            Luau::attachMagicFunction(ctv->props["Clone"].type(), magicFunctionInstanceClone);
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

    std::optional<RobloxDefinitionsFileMetadata> robloxMetadata = metadata;

    // Attach onto Instance.new()
    if (robloxMetadata.has_value() && !robloxMetadata->CREATABLE_INSTANCES.empty())
        if (auto instanceGlobal = globals.globalScope->lookup(Luau::AstName("Instance")))
            if (auto ttv = Luau::get<Luau::TableType>(instanceGlobal.value()))
                if (auto newFunction = ttv->props.find("new");
                    newFunction != ttv->props.end() && Luau::get<Luau::FunctionType>(newFunction->second.type()))
                {

                    Luau::attachTag(newFunction->second.type(), "CreatableInstances");
                    Luau::attachMagicFunction(
                        newFunction->second.type(), types::createMagicFunctionTypeLookup(robloxMetadata->CREATABLE_INSTANCES, "Invalid class name"));
                }

    // Attach onto `game:GetService()`
    if (robloxMetadata.has_value() && !robloxMetadata->SERVICES.empty())
        if (auto serviceProviderType = globals.globalScope->lookupType("ServiceProvider"))
            if (auto* ctv = Luau::getMutable<Luau::ClassType>(serviceProviderType->type);
                ctv && Luau::get<Luau::FunctionType>(ctv->props["GetService"].type()))
            {
                Luau::attachTag(ctv->props["GetService"].type(), "Services");
                Luau::attachMagicFunction(
                    ctv->props["GetService"].type(), types::createMagicFunctionTypeLookup(robloxMetadata->SERVICES, "Invalid service name"));
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
                    Luau::asMutable(ty)->documentationSymbol = "@lsp/enum/" + ctv->name;
                    for (auto& [name, prop] : ctv->props)
                    {
                        prop.documentationSymbol = "@lsp/enum/" + ctv->name + "." + name;
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
            Luau::ClassType baseInstanceCtv{typeName, {}, baseTypeId, instanceMetaIdentity, {}, {}, "@lsp"};
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
                    ctv->props["FindFirstAncestor"] = Luau::makeProperty(findFirstAncestorFunction, "@lsp/globaltype/Instance.FindFirstAncestor");

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
                    ctv->props["FindFirstChild"] = Luau::makeProperty(findFirstChildFunction, "@lsp/globaltype/Instance.FindFirstChild");
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

std::optional<Luau::AutocompleteEntryMap> RobloxPlatform::completionCallback(
    const std::string& tag, std::optional<const Luau::ClassType*> ctx, std::optional<std::string> contents, const Luau::ModuleName& moduleName)
{
    std::optional<RobloxDefinitionsFileMetadata> metadata = workspaceFolder->definitionsFileMetadata;

    if (tag == "ClassNames")
    {
        if (auto instanceType = workspaceFolder->frontend.globals.globalScope->lookupType("Instance"))
        {
            if (auto* ctv = Luau::get<Luau::ClassType>(instanceType->type))
            {
                Luau::AutocompleteEntryMap result;
                for (auto& [_, ty] : workspaceFolder->frontend.globals.globalScope->exportedTypeBindings)
                {
                    if (auto* c = Luau::get<Luau::ClassType>(ty.type))
                    {
                        // Check if the ctv is a subclass of instance
                        if (Luau::isSubclass(c, ctv))

                            result.insert_or_assign(
                                c->name, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String,
                                             workspaceFolder->frontend.builtinTypes->stringType, false, false, Luau::TypeCorrectKind::Correct});
                    }
                }

                return result;
            }
        }
    }
    else if (tag == "Properties")
    {
        if (ctx && ctx.value())
        {
            Luau::AutocompleteEntryMap result;
            auto ctv = ctx.value();
            while (ctv)
            {
                for (auto& [propName, prop] : ctv->props)
                {
                    // Don't include functions or events
                    auto ty = Luau::follow(prop.type());
                    if (Luau::get<Luau::FunctionType>(ty) || Luau::isOverloadedFunction(ty))
                        continue;
                    else if (auto ttv = Luau::get<Luau::TableType>(ty); ttv && ttv->name && ttv->name.value() == "RBXScriptSignal")
                        continue;

                    result.insert_or_assign(
                        propName, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType,
                                      false, false, Luau::TypeCorrectKind::Correct});
                }
                if (ctv->parent)
                    ctv = Luau::get<Luau::ClassType>(*ctv->parent);
                else
                    break;
            }
            return result;
        }
    }
    else if (tag == "Enums")
    {
        auto it = workspaceFolder->frontend.globals.globalScope->importedTypeBindings.find("Enum");
        if (it == workspaceFolder->frontend.globals.globalScope->importedTypeBindings.end())
            return std::nullopt;

        Luau::AutocompleteEntryMap result;
        for (auto& [enumName, _] : it->second)
            result.insert_or_assign(enumName, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String,
                                                  workspaceFolder->frontend.builtinTypes->stringType, false, false, Luau::TypeCorrectKind::Correct});

        return result;
    }
    else if (tag == "CreatableInstances")
    {
        Luau::AutocompleteEntryMap result;
        if (metadata)
        {
            for (const auto& className : metadata->CREATABLE_INSTANCES)
                result.insert_or_assign(
                    className, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType, false,
                                   false, Luau::TypeCorrectKind::Correct});
        }
        return result;
    }
    else if (tag == "Services")
    {
        Luau::AutocompleteEntryMap result;

        // We are autocompleting a `game:GetService("$1")` call, so we set a flag to
        // highlight this so that we can prioritise common services first in the list
        if (metadata)
        {
            for (const auto& className : metadata->SERVICES)
                result.insert_or_assign(
                    className, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType, false,
                                   false, Luau::TypeCorrectKind::Correct});
        }

        return result;
    }

    return std::nullopt;
}

const char* RobloxPlatform::handleSortText(
    const Luau::Frontend& frontend, const std::string& name, const Luau::AutocompleteEntry& entry, const std::unordered_set<std::string>& tags)
{
    // If it's a `game:GetSerivce("$1")` call, then prioritise common services
    if (tags.count("Services"))
        if (auto it = std::find(std::begin(COMMON_SERVICES), std::end(COMMON_SERVICES), name); it != std::end(COMMON_SERVICES))
            return SortText::PrioritisedSuggestion;

    // If calling a property on ServiceProvider, then prioritise these properties
    if (auto dataModelType = frontend.globalsForAutocomplete.globalScope->lookupType("ServiceProvider");
        dataModelType && Luau::get<Luau::ClassType>(dataModelType->type) && entry.containingClass &&
        Luau::isSubclass(entry.containingClass.value(), Luau::get<Luau::ClassType>(dataModelType->type)) && !entry.wrongIndexType)
    {
        if (auto it = std::find(std::begin(COMMON_SERVICE_PROVIDER_PROPERTIES), std::end(COMMON_SERVICE_PROVIDER_PROPERTIES), name);
            it != std::end(COMMON_SERVICE_PROVIDER_PROPERTIES))
            return SortText::PrioritisedSuggestion;
    }

    // If calling a property on an Instance, then prioritise these properties
    else if (auto instanceType = frontend.globalsForAutocomplete.globalScope->lookupType("Instance");
             instanceType && Luau::get<Luau::ClassType>(instanceType->type) && entry.containingClass &&
             Luau::isSubclass(entry.containingClass.value(), Luau::get<Luau::ClassType>(instanceType->type)) && !entry.wrongIndexType)
    {
        if (auto it = std::find(std::begin(COMMON_INSTANCE_PROPERTIES), std::end(COMMON_INSTANCE_PROPERTIES), name);
            it != std::end(COMMON_INSTANCE_PROPERTIES))
            return SortText::PrioritisedSuggestion;
    }

    return nullptr;
}

std::optional<lsp::CompletionItemKind> RobloxPlatform::handleEntryKind(const Luau::AutocompleteEntry& entry)
{
    if (entry.type.has_value())
    {
        auto id = Luau::follow(entry.type.value());

        if (auto ttv = Luau::get<Luau::TableType>(id))
        {
            // Special case the RBXScriptSignal type as a connection
            if (ttv->name && ttv->name.value() == "RBXScriptSignal")
                return lsp::CompletionItemKind::Event;
        }
    }

    return std::nullopt;
}

void RobloxPlatform::handleSuggestImports(const ClientConfiguration& config, FindImportsVisitor* importsVisitor, size_t hotCommentsLineNumber,
    bool isType, std::vector<lsp::CompletionItem>& items)
{
    if (!config.platform.roblox.suggestServices || !config.completion.imports.suggestServices || isType)
        return;

    // Suggest services
    auto robloxVisitor = dynamic_cast<RobloxFindImportsVisitor*>(importsVisitor);
    if (robloxVisitor == nullptr)
        return;

    std::optional<RobloxDefinitionsFileMetadata> metadata = workspaceFolder->definitionsFileMetadata;

    auto services = metadata.has_value() ? metadata->SERVICES : std::vector<std::string>{};
    for (auto& service : services)
    {
        // ASSUMPTION: if the service was defined, it was defined with the exact same name
        if (contains(robloxVisitor->serviceLineMap, service))
            continue;

        size_t lineNumber = robloxVisitor->findBestLineForService(service, hotCommentsLineNumber);

        bool appendNewline = false;
        if (config.completion.imports.separateGroupsWithLine && importsVisitor->firstRequireLine &&
            importsVisitor->firstRequireLine.value() - lineNumber == 0)
            appendNewline = true;

        items.emplace_back(createSuggestService(service, lineNumber, appendNewline));
    }
}

void RobloxPlatform::handleRequire(const std::string& requirePath, size_t lineNumber, bool isRelative, const ClientConfiguration& config,
    FindImportsVisitor* importsVisitor, size_t hotCommentsLineNumber, std::vector<lsp::TextEdit>& textEdits)
{
    if (!isRelative)
        return;

    auto robloxVisitor = dynamic_cast<RobloxFindImportsVisitor*>(importsVisitor);
    if (robloxVisitor == nullptr)
        return;

    // Service will be the first part of the path
    // If we haven't imported the service already, then we auto-import it
    auto service = requirePath.substr(0, requirePath.find('/'));
    if (!contains(robloxVisitor->serviceLineMap, service))
    {
        auto lineNumber = robloxVisitor->findBestLineForService(service, hotCommentsLineNumber);
        bool appendNewline = false;
        // If there is no firstRequireLine, then the require that we insert will become the first require,
        // so we use `.value_or(lineNumber)` to ensure it equals 0 and a newline is added
        if (config.completion.imports.separateGroupsWithLine && robloxVisitor->firstRequireLine.value_or(lineNumber) - lineNumber == 0)
            appendNewline = true;
        textEdits.emplace_back(createServiceTextEdit(service, lineNumber, appendNewline));
    }
}

lsp::WorkspaceEdit RobloxPlatform::computeOrganiseServicesEdit(const lsp::DocumentUri& uri)
{
    auto moduleName = workspaceFolder->fileResolver.getModuleName(uri);
    auto textDocument = workspaceFolder->fileResolver.getTextDocument(uri);

    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + uri.toString());

    workspaceFolder->frontend.parse(moduleName);

    auto sourceModule = workspaceFolder->frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return {};

    // Find all `local X = game:GetService("Service")`
    RobloxFindImportsVisitor visitor;
    visitor.visit(sourceModule->root);

    if (visitor.serviceLineMap.empty())
        return {};

    // Test to see that if all the services are already sorted -> if they are, then just leave alone
    // to prevent clogging the undo history stack
    Luau::Location previousServiceLocation{{0, 0}, {0, 0}};
    bool isSorted = true;
    for (const auto& [_, stat] : visitor.serviceLineMap)
    {
        if (stat->location.begin < previousServiceLocation.begin)
        {
            isSorted = false;
            break;
        }
        previousServiceLocation = stat->location;
    }
    if (isSorted)
        return {};

    std::vector<lsp::TextEdit> edits;
    // We firstly delete all the previous services, as they will be added later
    edits.reserve(visitor.serviceLineMap.size());
    for (const auto& [_, stat] : visitor.serviceLineMap)
        edits.emplace_back(lsp::TextEdit{{{stat->location.begin.line, 0}, {stat->location.begin.line + 1, 0}}, ""});

    // We find the first line to add these services to, and then add them in sorted order
    lsp::Range insertLocation{{visitor.firstServiceDefinitionLine.value(), 0}, {visitor.firstServiceDefinitionLine.value(), 0}};
    for (const auto& [serviceName, stat] : visitor.serviceLineMap)
    {
        // We need to rewrite the statement as we expected it
        auto importText = Luau::toString(stat) + "\n";
        edits.emplace_back(lsp::TextEdit{insertLocation, importText});
    }

    lsp::WorkspaceEdit workspaceEdit;
    workspaceEdit.changes.emplace(uri.toString(), edits);
    return workspaceEdit;
}

void RobloxPlatform::handleCodeAction(const lsp::CodeActionParams& params, std::vector<lsp::CodeAction>& items)
{
    if (params.context.wants(lsp::CodeActionKind::Source) || params.context.wants(lsp::CodeActionKind::SourceOrganizeImports))
    {
        lsp::CodeAction sortServicesAction;
        sortServicesAction.title = "Sort services";
        sortServicesAction.kind = lsp::CodeActionKind::SourceOrganizeImports;
        sortServicesAction.edit = computeOrganiseServicesEdit(params.textDocument.uri);
        items.emplace_back(sortServicesAction);
    }
}

std::optional<lsp::ColorInformation> RobloxPlatform::colorInformation(Luau::AstExprCall* call, const TextDocument* textDocument)
{
    auto index = call->func->as<Luau::AstExprIndexName>();
    if (!index)
        return std::nullopt;

    auto global = index->expr->as<Luau::AstExprGlobal>();
    if (!global || global->name != "Color3")
        return std::nullopt;

    std::array<double, 3> color = {0.0, 0.0, 0.0};

    if (index->index == "new")
    {
        size_t argIndex = 0;
        for (auto arg : call->args)
        {
            if (argIndex >= 3)
                return std::nullopt; // Don't create as the colour is not in the right format
            if (auto number = arg->as<Luau::AstExprConstantNumber>())
                color.at(argIndex) = number->value;
            else
                return std::nullopt; // Don't create as we can't parse the full colour
            argIndex++;
        }
    }
    else if (index->index == "fromRGB")
    {
        size_t argIndex = 0;
        for (auto arg : call->args)
        {
            if (argIndex >= 3)
                return std::nullopt; // Don't create as the colour is not in the right format
            if (auto number = arg->as<Luau::AstExprConstantNumber>())
                color.at(argIndex) = number->value / 255.0;
            else
                return std::nullopt; // Don't create as we can't parse the full colour
            argIndex++;
        }
    }
    else if (index->index == "fromHSV")
    {
        size_t argIndex = 0;
        for (auto arg : call->args)
        {
            if (argIndex >= 3)
                return std::nullopt; // Don't create as the colour is not in the right format
            if (auto number = arg->as<Luau::AstExprConstantNumber>())
                color.at(argIndex) = number->value;
            else
                return std::nullopt; // Don't create as we can't parse the full colour
            argIndex++;
        }
        RGB data = hsvToRgb({color[0], color[1], color[2]});
        color[0] = data.r / 255.0;
        color[1] = data.g / 255.0;
        color[2] = data.b / 255.0;
    }
    else if (index->index == "fromHex")
    {
        if (call->args.size != 1)
            return std::nullopt; // Don't create as the colour is not in the right format

        if (auto string = call->args.data[0]->as<Luau::AstExprConstantString>())
        {
            try
            {
                RGB data = hexToRgb(std::string(string->value.data, string->value.size));
                color[0] = data.r / 255.0;
                color[1] = data.g / 255.0;
                color[2] = data.b / 255.0;
            }
            catch (const std::exception&)
            {
                return std::nullopt; // Invalid hex string
            }
        }
        else
            return std::nullopt; // Don't create as we can't parse the full colour
    }
    else
    {
        return std::nullopt;
    }

    return lsp::ColorInformation{lsp::Range{textDocument->convertPosition(call->location.begin), textDocument->convertPosition(call->location.end)},
        {std::clamp(color[0], 0.0, 1.0), std::clamp(color[1], 0.0, 1.0), std::clamp(color[2], 0.0, 1.0), 1.0}};
}

lsp::ColorPresentationResult RobloxPlatform::colorPresentation(const lsp::ColorPresentationParams& params)
{
    // Create color presentations
    lsp::ColorPresentationResult presentations;

    // Add Color3.new
    presentations.emplace_back(lsp::ColorPresentation{"Color3.new(" + std::to_string(params.color.red) + ", " + std::to_string(params.color.green) +
                                                      ", " + std::to_string(params.color.blue) + ")"});

    // Convert to RGB values
    RGB rgb{
        (int)std::floor(params.color.red * 255.0),
        (int)std::floor(params.color.green * 255.0),
        (int)std::floor(params.color.blue * 255.0),
    };

    // Add Color3.fromRGB
    presentations.emplace_back(
        lsp::ColorPresentation{"Color3.fromRGB(" + std::to_string(rgb.r) + ", " + std::to_string(rgb.g) + ", " + std::to_string(rgb.b) + ")"});

    // Add Color3.fromHSV
    HSV hsv = rgbToHsv(rgb);
    presentations.emplace_back(
        lsp::ColorPresentation{"Color3.fromHSV(" + std::to_string(hsv.h) + ", " + std::to_string(hsv.s) + ", " + std::to_string(hsv.v) + ")"});

    // Add Color3.fromHex
    presentations.emplace_back(lsp::ColorPresentation{"Color3.fromHex(\"" + rgbToHex(rgb) + "\")"});

    return presentations;
}

void RobloxPlatform::onStudioPluginFullChange(const PluginNode& dataModel)
{
    workspaceFolder->client->sendLogMessage(lsp::MessageType::Info, "received full change from studio plugin");

    // TODO: properly handle multi-workspace setup
    pluginInfo = std::make_shared<PluginNode>(dataModel);

    // Mutate the sourcemap with the new information
    workspaceFolder->updateSourceMap();
}

void RobloxPlatform::onStudioPluginClear()
{
    workspaceFolder->client->sendLogMessage(lsp::MessageType::Info, "received clear from studio plugin");

    // TODO: properly handle multi-workspace setup
    pluginInfo = nullptr;

    // Mutate the sourcemap with the new information
    workspaceFolder->updateSourceMap();
}

bool RobloxPlatform::handleNotification(const std::string& method, std::optional<json> params)
{
    if (method == "$/plugin/full")
    {
        onStudioPluginFullChange(JSON_REQUIRED_PARAMS(params, "$/plugin/full"));
    }
    else if (method == "$/plugin/clear")
    {
        onStudioPluginClear();
    }

    return false;
}
