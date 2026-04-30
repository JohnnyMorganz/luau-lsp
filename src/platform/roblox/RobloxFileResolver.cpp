#include "Platform/RobloxPlatform.hpp"
#include "Platform/RobloxStringRequireSuggester.hpp"
#include "Platform/StringRequireAutoImporter.hpp"
#include "LSP/JsonTomlSyntaxParser.hpp"
#include "LSP/Completion.hpp"
#include "LSP/Utils.hpp"

#include "Luau/Config.h"
#include "Luau/TimeTrace.h"

#include <unordered_set>
#include "LuauFileUtils.hpp"

std::optional<Luau::ModuleName> RobloxPlatform::resolveToVirtualPath(const Uri& uri) const
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::resolveToVirtualPath", "LSP");
    if (const auto sourceNode = getSourceNodeFromRealPath(uri))
        return getVirtualPathFromSourceNode(*sourceNode);
    return std::nullopt;
}

std::optional<Uri> RobloxPlatform::resolveToRealPath(const Luau::ModuleName& name) const
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::resolveToRealPath", "LSP");
    if (isVirtualPath(name))
    {
        if (auto sourceNode = getSourceNodeFromVirtualPath(name))
        {
            return getRealPathFromSourceNode(*sourceNode);
        }
    }
    else
    {
        return fileResolver->getUri(name);
    }

    return std::nullopt;
}

Luau::SourceCode::Type RobloxPlatform::sourceCodeTypeFromPath(const Uri& path) const
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::sourceCodeTypeFromPath", "LSP");
    if (auto sourceNode = getSourceNodeFromRealPath(path))
        return (*sourceNode)->sourceCodeType();

    auto filename = path.filename();

    if (endsWith(filename, ".server.lua") || endsWith(filename, ".server.luau") || endsWith(filename, ".client.lua") ||
        endsWith(filename, ".client.luau"))
    {
        return Luau::SourceCode::Type::Script;
    }

    return Luau::SourceCode::Type::Module;
}

std::optional<std::string> RobloxPlatform::readSourceCode(const Luau::ModuleName& name, const Uri& path) const
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::readSourceCode", "LSP");
    if (auto parentResult = LSPPlatform::readSourceCode(name, path))
        return parentResult;

    auto source = Luau::FileUtils::readFile(path.fsPath());
    if (!source)
        return std::nullopt;

    if (path.extension() == ".json")
    {
        try
        {
            source = "--!strict\nreturn " + jsonValueToLuau(json::parse(*source));
        }
        catch (const std::exception& e)
        {
            // TODO: display diagnostic?
            std::cerr << "Failed to load JSON module: " << path.toString() << " - " << e.what() << '\n';
            return std::nullopt;
        }
    }
    else if (path.extension() == ".toml")
    {
        try
        {
            std::string tomlSource(*source);
            std::istringstream tomlSourceStream(tomlSource, std::ios_base::binary | std::ios_base::in);
            source = "--!strict\nreturn " + tomlValueToLuau(toml::parse(tomlSourceStream, path.fsPath()));
        }
        catch (const std::exception& e)
        {
            // TODO: display diagnostic?
            std::cerr << "Failed to load TOML module: " << path.toString() << " - " << e.what() << '\n';
            return std::nullopt;
        }
    }
    else if (path.extension() == ".yaml" || path.extension() == ".yml")
    {
        try
        {
            ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(*source));
            source = "--!strict\nreturn " + yamlValueToLuau(tree.rootref());
        }
        catch (const std::exception& e)
        {
            // TODO: display diagnostic?
            std::cerr << "Failed to load YAML module: " << path.toString() << " - " << e.what() << '\n';
            return std::nullopt;
        }
    }

    return source;
}

std::optional<Luau::ModuleInfo> RobloxPlatform::resolveStringRequire(
    const Luau::ModuleInfo* context, const std::string& requiredString, const Luau::TypeCheckLimits& limits)
{
    if (!context)
        return std::nullopt;

    if (!isVirtualPath(context->name))
        return LSPPlatform::resolveStringRequire(context, requiredString, limits);

    if (!requiredString.empty() && requiredString[0] == '@')
    {
        const auto& luauConfig = fileResolver->getConfig(context->name, limits);

        size_t slashPos = requiredString.find('/');
        std::string aliasName = requiredString.substr(1, slashPos == std::string::npos ? std::string::npos : slashPos - 1);
        std::string aliasNameLower = toLower(aliasName);

        if (luauConfig.aliases.find(aliasNameLower) || aliasNameLower == "self")
            return LSPPlatform::resolveStringRequire(context, requiredString, limits);

        if (aliasNameLower == "game" && rootSourceNode)
        {
            std::string remainder = (slashPos == std::string::npos) ? "" : requiredString.substr(slashPos + 1);
            if (remainder.empty())
                return Luau::ModuleInfo{rootSourceNode->virtualPath};
            return Luau::ModuleInfo{rootSourceNode->virtualPath + "/" + remainder};
        }

        return std::nullopt;
    }

    auto parentPath = getParentPath(context->name);
    if (!parentPath)
        return std::nullopt;

    // Walk the path using string manipulation rather than SourceNode::walkPath because
    // validation is handled downstream — walkPath rejects missing children via findChild.
    std::string base = *parentPath;
    size_t start = 0;
    while (start < requiredString.size())
    {
        if (requiredString.compare(start, 2, "./") == 0)
        {
            start += 2;
            continue;
        }

        size_t end = requiredString.find('/', start);
        std::string segment = (end == std::string::npos) ? requiredString.substr(start) : requiredString.substr(start, end - start);
        start = (end == std::string::npos) ? requiredString.size() : end + 1;

        if (segment.empty() || segment == ".")
            continue;

        if (segment == "..")
        {
            auto parent = getParentPath(base);
            if (!parent)
                return std::nullopt;
            base = *parent;
        }
        else
        {
            base += '/';
            base += segment;
        }
    }

    return Luau::ModuleInfo{base};
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

std::unique_ptr<Luau::RequireSuggester> RobloxPlatform::getRequireSuggester()
{
    return std::make_unique<RobloxStringRequireSuggester>(workspaceFolder, fileResolver, this);
}

static const SourceNode* getServiceNode(const SourceNode* n)
{
    const SourceNode* prev = n;
    while (n && n->parent)
    {
        prev = n;
        n = n->parent;
    }
    return prev;
}

static std::optional<std::pair<std::string, const char*>> computeSourcemapRequirePath(
    const RobloxPlatform* platform,
    const SourceNode* fromNode,
    const std::unordered_set<const SourceNode*>& fromAncestors,
    const SourceNode* fromService,
    const Luau::ModuleName& target,
    ImportRequireStyle style,
    const Luau::LanguageServer::AutoImports::AliasMap& availableAliases)
{
    auto targetIt = platform->virtualPathsToSourceNodes.find(target);
    if (targetIt == platform->virtualPathsToSourceNodes.end())
        return std::nullopt;

    const SourceNode* targetNode = targetIt->second;

    if (!targetNode->isScript())
        return std::nullopt;

    // Compute absolute path: prefer user-defined aliases, then fall back to @game/<virtual path>
    auto computeAbsolute = [&]() -> std::pair<std::string, const char*>
    {
        if (auto realPath = platform->getRealPathFromSourceNode(targetNode))
        {
            if (auto aliasPath = Luau::LanguageServer::AutoImports::computeBestAliasedPath(*realPath, availableAliases))
                return {*aliasPath, SortText::AutoImportsAbsolute};
        }

        std::string path = targetNode->virtualPath;
        if (Luau::startsWith(path, "game/"))
            path = path.substr(5);
        return {"@game/" + path, SortText::AutoImportsAbsolute};
    };

    if (style == ImportRequireStyle::AlwaysAbsolute)
        return computeAbsolute();

    // Find lowest common ancestor
    const SourceNode* commonAncestor = nullptr;
    for (auto n = targetNode; n; n = n->parent)
    {
        if (fromAncestors.count(n))
        {
            commonAncestor = n;
            break;
        }
    }
    if (!commonAncestor)
        return computeAbsolute();

    // Count hops up from fromNode->parent to common ancestor.
    // Safe: commonAncestor is guaranteed to be in fromAncestors, so
    // n will reach it before becoming nullptr.
    int hopsUp = 0;
    for (auto n = fromNode->parent; n != commonAncestor; n = n->parent)
        hopsUp++;

    // Build path down from common ancestor to target
    std::vector<std::string> pathDown;
    for (auto n = targetNode; n != commonAncestor; n = n->parent)
        pathDown.push_back(n->name);
    std::reverse(pathDown.begin(), pathDown.end());

    std::string relativePath;
    if (hopsUp == 0)
        relativePath = "./";
    else
        for (int i = 0; i < hopsUp; i++)
            relativePath += "../";

    for (size_t i = 0; i < pathDown.size(); i++)
    {
        if (i > 0)
            relativePath += "/";
        relativePath += pathDown[i];
    }

    if (style == ImportRequireStyle::AlwaysRelative)
        return std::pair{relativePath, SortText::AutoImports};

    // Auto: use relative if same service subtree, otherwise @game
    if (fromService == getServiceNode(targetNode))
        return std::pair{relativePath, SortText::AutoImports};

    return computeAbsolute();
}

Luau::LanguageServer::AutoImports::ModuleVisitor RobloxPlatform::getAutoImportsModuleVisitor(const Luau::ModuleName& from)
{
    if (!rootSourceNode || virtualPathsToSourceNodes.find(from) == virtualPathsToSourceNodes.end())
        return LSPPlatform::getAutoImportsModuleVisitor(from);

    return [this](const std::function<void(const Luau::ModuleName&)>& visit)
    {
        for (const auto& [name, _] : virtualPathsToSourceNodes)
            visit(name);
    };
}

std::optional<Luau::LanguageServer::AutoImports::RequirePathComputer> RobloxPlatform::getAutoImportsRequirePathComputer(
    const Luau::ModuleName& from, ImportRequireStyle style)
{
    if (!rootSourceNode)
        return std::nullopt;

    auto fromIt = virtualPathsToSourceNodes.find(from);
    if (fromIt == virtualPathsToSourceNodes.end())
        return std::nullopt;

    const SourceNode* fromNode = fromIt->second;

    // Precompute ancestors of fromNode once, reused across all candidate modules
    std::unordered_set<const SourceNode*> fromAncestors;
    for (auto n = fromNode->parent; n; n = n->parent)
        fromAncestors.insert(n);
    const SourceNode* fromService = getServiceNode(fromNode);

    auto availableAliases = fileResolver->getConfig(from, workspaceFolder->limits).aliases;

    return [this, fromNode, fromAncestors = std::move(fromAncestors), fromService, style,
               availableAliases = std::move(availableAliases)](
               const Luau::ModuleName& /*from*/, const Luau::ModuleName& target)
        -> std::optional<std::pair<std::string, const char*>>
    {
        return computeSourcemapRequirePath(this, fromNode, fromAncestors, fromService, target, style, availableAliases);
    };
}

std::optional<Luau::ModuleInfo> RobloxPlatform::resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* node, const Luau::TypeCheckLimits& limits)
{

    if (auto parentResult = LSPPlatform::resolveModule(context, node, limits))
        return parentResult;

    if (auto* g = node->as<Luau::AstExprGlobal>())
    {
        if (g->name == "game")
            return Luau::ModuleInfo{"game"};

        if (g->name == "script")
        {
            if (isVirtualPath(context->name))
            {
                return Luau::ModuleInfo{context->name};
            }
            else if (auto virtualPath = resolveToVirtualPath(fileResolver->getUri(context->name)))
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
