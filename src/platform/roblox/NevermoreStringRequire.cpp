#include "Platform/RobloxPlatform.hpp"
#include "LSP/JsonTomlSyntaxParser.hpp"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/ConstraintSolver.h"
#include "Luau/TypeInfer.h"

#include <filesystem>
#include <queue>

#ifdef NEVERMORE_STRING_REQUIRE

struct MagicStringRequireLookup final : Luau::MagicFunction
{
    const Luau::GlobalTypes& globals;
    const RobloxPlatform& platform;
    Luau::TypeArena& arena;
    const SourceNode* node;

    MagicStringRequireLookup(const Luau::GlobalTypes& globals, const RobloxPlatform& platform, Luau::TypeArena& arena, const SourceNode* node)
        : globals(globals)
        , platform(platform)
        , arena(arena)
        , node(std::move(node))
    {
    }

    std::optional<Luau::WithPredicate<Luau::TypePackId>> handleOldSolver(Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope,
        const Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId> withPredicate) override;
    bool infer(const Luau::MagicFunctionCallContext& context) override;
};

std::optional<Luau::WithPredicate<Luau::TypePackId>> MagicStringRequireLookup::handleOldSolver(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::WithPredicate<Luau::TypePackId>)
{
    if (expr.args.size < 1)
    {
        typeChecker.reportError(Luau::TypeError{expr.args.data[0]->location, Luau::UnknownRequire{}});
        return std::nullopt;
    }

    auto str = expr.args.data[0]->as<Luau::AstExprConstantString>();
    if (!str)
    {
        typeChecker.reportError(Luau::TypeError{expr.args.data[0]->location, Luau::UnknownRequire{}});
        return std::nullopt;
    }

    auto moduleName = std::string(str->value.data, str->value.size);

    if (node->name == moduleName)
    {
        typeChecker.reportError(Luau::TypeError{expr.args.data[0]->location, Luau::UnknownRequire{ moduleName }});
        return std::nullopt;
    }

    auto module = platform.findStringModule(moduleName);
    if (!module.has_value())
    {
        typeChecker.reportError(Luau::TypeError{expr.args.data[0]->location, Luau::UnknownRequire{ moduleName }});
        return std::nullopt;
    }

    Luau::ModuleInfo moduleInfo;
    moduleInfo.name = module.value()->virtualPath;

    return Luau::WithPredicate<Luau::TypePackId>{arena.addTypePack({typeChecker.checkRequire(scope, moduleInfo, expr.args.data[0]->location)})};
}

bool MagicStringRequireLookup::infer(const Luau::MagicFunctionCallContext& context)
{
    // TODO: Actually like, do something here
    if (context.callSite->args.size < 1)
        return false;

    auto str = context.callSite->args.data[0]->as<Luau::AstExprConstantString>();
    if (!str)
        return false;

    auto moduleName = std::string(str->value.data, str->value.size);
    auto module = platform.findStringModule(moduleName);
    if (!module.has_value())
    {
        context.solver->reportError(Luau::UnknownRequire{ moduleName }, context.callSite->args.data[0]->location);
        return false;
    }


    Luau::ModuleInfo moduleInfo;
    moduleInfo.name = module.value()->virtualPath;

    asMutable(context.result)->ty.emplace<Luau::BoundTypePack>(context.solver->arena->addTypePack({
        context.solver->resolveModule(moduleInfo, context.callSite->args.data[0]->location)
    }));

    return true;
}

static void attachMagicStringRequireLookupFunction(const Luau::GlobalTypes& globals, const RobloxPlatform& platform, Luau::TypeArena& arena, const SourceNode* node, Luau::TypeId lookupFuncTy)
{

    Luau::attachMagicFunction(
        lookupFuncTy, std::make_shared<MagicStringRequireLookup>(globals, platform, arena, node));
    Luau::attachTag(lookupFuncTy, kSourcemapGeneratedTag);
    Luau::attachTag(lookupFuncTy, "StringRequires");
    Luau::attachTag(lookupFuncTy, "require"); // Magic tag
}

Luau::TypeId RobloxPlatform::getStringRequireType(const Luau::GlobalTypes& globals, Luau::TypeArena& arena, const SourceNode* node) const
{
    // Gets the type corresponding to the sourcemap node if it exists
    // Make sure to use the correct ty version (base typeChecker vs autocomplete typeChecker)
    if (node->stringRequireTypes.find(&globals) != node->stringRequireTypes.end())
        return node->stringRequireTypes.at(&globals);

    // TODO: Memory safety for RobloxPlatform this

    Luau::LazyType lazyTypeValue(
        [&globals, this, &arena, node](Luau::LazyType& lazyTypeValue) -> void
        {
            // Check if the lazy type value already has an unwrapped type
            if (lazyTypeValue.unwrapped.load())
                return;

            // Handle if the node is no longer valid
            if (!node)
            {
                lazyTypeValue.unwrapped = globals.builtinTypes->anyType;
                return;
            }

            // TODO: Resolve name to lazy instance
            // Or type union
            Luau::TypePackId argTypes = arena.addTypePack({ globals.builtinTypes->stringType });
            Luau::TypePackId retTypes = arena.addTypePack({ globals.builtinTypes->anyType }); // This should be overriden by the type checker
            Luau::FunctionType functionCtv(argTypes, retTypes);

            auto typeId = arena.addType(std::move(functionCtv));
            attachMagicStringRequireLookupFunction(globals, *this, arena, node, typeId);

            lazyTypeValue.unwrapped = typeId;
            return;
        });

    auto ty = arena.addType(std::move(lazyTypeValue));
    node->stringRequireTypes.insert_or_assign(&globals, ty);

    return ty;
}

std::optional<const SourceNode*> RobloxPlatform::findStringModule(const std::string& moduleName) const
{
    // TODO: Use "node_modules" as a project scope and handle duplications
    auto result = this->moduleNameToSourceNode.find(moduleName);
    if (result != this->moduleNameToSourceNode.end())
        return result->second;

    return std::nullopt;
}

std::optional<std::string> RobloxPlatform::resolveToVirtualSourceCode(const Luau::ModuleName& name) const
{
    if (!isVirtualPath(name))
    {
        return std::nullopt;
    }

    auto sourceNode = getSourceNodeFromVirtualPath(name);
    if (!sourceNode || !sourceNode.value()->isVirtualNevermoreLoader)
    {
        return std::nullopt;
    }

    std::string source = R"lua(
--!strict

local loader = {}

function loader.load(thisScript: ModuleScript): typeof(StringRequire)
    return nil :: never
end

return loader
)lua";

    return source;
}
#endif