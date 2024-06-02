#include "Platform/RobloxPlatform.hpp"
#include "LSP/FileUtils.h"

std::optional<Luau::ModuleName> RobloxPlatform::resolveToVirtualPath(const std::string& name) const
{
    if (isVirtualPath(name))
    {
        return name;
    }
    else
    {
        auto sourceNode = getSourceNodeFromRealPath(name);
        if (!sourceNode)
            return std::nullopt;
        return getVirtualPathFromSourceNode(sourceNode.value());
    }
}

std::optional<std::filesystem::path> RobloxPlatform::resolveToRealPath(const Luau::ModuleName& name) const
{
    if (isVirtualPath(name))
    {
        if (auto sourceNode = getSourceNodeFromVirtualPath(name))
        {
            return getRealPathFromSourceNode(*sourceNode);
        }
    }
    else
    {
        return name;
    }

    return std::nullopt;
}

Luau::SourceCode::Type RobloxPlatform::sourceCodeTypeFromPath(const std::filesystem::path& path) const
{
    if (auto sourceNode = getSourceNodeFromRealPath(path.generic_string()))
        return (*sourceNode)->sourceCodeType();

    auto filename = path.filename().generic_string();

    if (endsWith(filename, ".server.lua") || endsWith(filename, ".server.luau"))
    {
        return Luau::SourceCode::Type::Script;
    }
    else if (endsWith(filename, ".client.lua") || endsWith(filename, ".client.luau"))
    {
        return Luau::SourceCode::Type::Local;
    }

    return Luau::SourceCode::Type::Module;
}

static std::string jsonValueToLuau(const json& val)
{
    if (val.is_string() || val.is_number() || val.is_boolean())
    {
        return val.dump();
    }
    else if (val.is_null())
    {
        return "nil";
    }
    else if (val.is_array())
    {
        std::string out = "{";
        for (auto& elem : val)
        {
            out += jsonValueToLuau(elem);
            out += ";";
        }

        out += "}";
        return out;
    }
    else if (val.is_object())
    {
        std::string out = "{";

        for (auto& [key, value] : val.items())
        {
            out += "[\"" + key + "\"] = ";
            out += jsonValueToLuau(value);
            out += ";";
        }

        out += "}";
        return out;
    }
    else
    {
        return ""; // TODO: should we error here?
    }
}

std::optional<std::string> RobloxPlatform::readSourceCode(const Luau::ModuleName& name, const std::filesystem::path& path) const
{
    if (auto parentResult = LSPPlatform::readSourceCode(name, path))
        return parentResult;

    auto source = readFile(path);
    if (source && path.extension() == ".json")
    {
        try
        {
            source = "--!strict\nreturn " + jsonValueToLuau(json::parse(*source));
        }
        catch (const std::exception& e)
        {
            // TODO: display diagnostic?
            std::cerr << "Failed to load JSON module: " << path.generic_string() << " - " << e.what() << '\n';
            return std::nullopt;
        }
    }

    return std::nullopt;
}

/// Modify the context so that game/Players/LocalPlayer items point to the correct place
static std::string mapContext(const std::string& context)
{
    if (context == "game/Players/LocalPlayer/PlayerScripts")
        return "game/StarterPlayer/StarterPlayerScripts";
    else if (context == "game/Players/LocalPlayer/PlayerGui")
        return "game/StarterGui";
    else if (context == "game/Players/LocalPlayer/StarterGear")
        return "game/StarterPack";
    return context;
}

std::optional<Luau::ModuleInfo> RobloxPlatform::resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* node)
{

    if (auto parentResult = LSPPlatform::resolveModule(context, node))
        return parentResult;

    if (auto* g = node->as<Luau::AstExprGlobal>())
    {
        if (g->name == "game")
            return Luau::ModuleInfo{"game"};

        if (g->name == "script")
        {
            if (auto virtualPath = resolveToVirtualPath(context->name))
            {
                return Luau::ModuleInfo{virtualPath.value()};
            }
        }
    }
    else if (auto* i = node->as<Luau::AstExprIndexName>())
    {
        if (context)
        {
            if (strcmp(i->index.value, "Parent") == 0)
            {
                // Pop the name instead
                auto parentPath = getParentPath(context->name);
                if (parentPath.has_value())
                    return Luau::ModuleInfo{parentPath.value(), context->optional};
            }

            return Luau::ModuleInfo{mapContext(context->name) + '/' + i->index.value, context->optional};
        }
    }
    else if (auto* i_expr = node->as<Luau::AstExprIndexExpr>())
    {
        if (auto* index = i_expr->index->as<Luau::AstExprConstantString>())
        {
            if (context)
                return Luau::ModuleInfo{mapContext(context->name) + '/' + std::string(index->value.data, index->value.size), context->optional};
        }
    }
    else if (auto* call = node->as<Luau::AstExprCall>(); call && call->self && call->args.size >= 1 && context)
    {
        if (auto* index = call->args.data[0]->as<Luau::AstExprConstantString>())
        {
            Luau::AstName func = call->func->as<Luau::AstExprIndexName>()->index;

            if (func == "GetService" && context->name == "game")
            {
                return Luau::ModuleInfo{"game/" + std::string(index->value.data, index->value.size)};
            }
            else if (func == "WaitForChild" || (func == "FindFirstChild" && call->args.size == 1)) // Don't allow recursive FFC
            {
                return Luau::ModuleInfo{mapContext(context->name) + '/' + std::string(index->value.data, index->value.size), context->optional};
            }
            else if (func == "FindFirstAncestor")
            {
                auto ancestorName = getAncestorPath(context->name, std::string(index->value.data, index->value.size), rootSourceNode);
                if (ancestorName)
                    return Luau::ModuleInfo{*ancestorName, context->optional};
            }
        }
    }

    return std::nullopt;
}
