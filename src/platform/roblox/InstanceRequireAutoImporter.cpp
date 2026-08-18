#include "Platform/InstanceRequireAutoImporter.hpp"

#include "LSP/Completion.hpp"
#include "Platform/RobloxPlatform.hpp"

#include <algorithm>

namespace Luau::LanguageServer::AutoImports
{

lsp::TextEdit createServiceTextEdit(const std::string& name, size_t lineNumber, bool appendNewline, bool useConst)
{
    auto range = lsp::Range{{lineNumber, 0}, {lineNumber, 0}};
    auto importText = std::string(declarationKeyword(useConst)) + name + " = game:GetService(\"" + name + "\")\n";
    if (appendNewline)
        importText += "\n";
    return {range, importText};
}

std::string optimiseAbsoluteRequire(const std::string& path)
{
    if (!Luau::startsWith(path, "game/"))
        return path;

    auto parts = Luau::split(path, '/');
    if (parts.size() > 2)
    {
        auto service = std::string(parts[1]);
        return service + "/" + Luau::join(std::vector(parts.begin() + 2, parts.end()), "/");
    }

    return path;
}

static std::optional<Luau::ModuleName> resolveFixedVariablePath(
    RobloxPlatform& platform, const Luau::ModuleName& from, Luau::AstExpr* expr, const Luau::TypeCheckLimits& limits)
{
    Luau::AstExpr* dependent = nullptr;
    if (auto* index = expr->as<Luau::AstExprIndexName>())
        dependent = index->expr;
    else if (auto* indexExpr = expr->as<Luau::AstExprIndexExpr>())
        dependent = indexExpr->expr;
    else if (auto* call = expr->as<Luau::AstExprCall>(); call && call->self)
    {
        if (auto* func = call->func->as<Luau::AstExprIndexName>())
            dependent = func->expr;
        else
            return std::nullopt;
    }

    std::optional<Luau::ModuleInfo> info;
    if (dependent)
    {
        auto context = resolveFixedVariablePath(platform, from, dependent, limits);
        if (!context)
            return std::nullopt;
        Luau::ModuleInfo contextInfo{*context};
        info = platform.resolveModule(&contextInfo, expr, limits);
    }
    else
    {
        Luau::ModuleInfo moduleContext{from};
        info = platform.resolveModule(&moduleContext, expr, limits);
    }

    if (info && !info->name.empty())
        return info->name;
    return std::nullopt;
}

struct ResolvedFixedVariable
{
    Luau::ModuleName path;
    std::string variableName;
    size_t endLine = 0;
};

std::vector<InstanceRequireResult> computeAllInstanceRequires(const InstanceRequireAutoImporterContext& ctx)
{
    std::vector<InstanceRequireResult> results;
    size_t minimumLineNumber = computeMinimumLineNumberForRequire(*ctx.importsVisitor, ctx.hotCommentsLineNumber);

    std::vector<ResolvedFixedVariable> resolvedFixedVariables;
    if (ctx.config->requireStyle == ImportRequireStyle::NearestAbsolute)
    {
        for (const auto& fixedVariable : ctx.importsVisitor->fixedVariables)
        {
            if (auto resolved = resolveFixedVariablePath(*ctx.platform, ctx.from, fixedVariable.expr, ctx.workspaceFolder->limits))
                resolvedFixedVariables.push_back({*resolved, fixedVariable.variableName, fixedVariable.endLine});
        }
    }

    ScriptContext callerContext = ScriptContext::Shared;
    if (auto it = ctx.platform->virtualPathsToSourceNodes.find(ctx.from); it != ctx.platform->virtualPathsToSourceNodes.end())
        callerContext = it->second->scriptContext;

    for (auto& [path, node] : ctx.platform->virtualPathsToSourceNodes)
    {
        auto name = AutoImports::makeValidVariableName(node->name);

        if (ctx.moduleFilter && !(*ctx.moduleFilter)(name))
            continue;

        if (path == ctx.from || node->className != "ModuleScript" || ctx.importsVisitor->containsRequire(name))
            continue;
        if (auto scriptFilePath = ctx.platform->getRealPathFromSourceNode(node);
            scriptFilePath && ctx.workspaceFolder->isIgnoredFileForAutoImports(*scriptFilePath))
            continue;

        if (!isScriptContextCompatible(callerContext, node->scriptContext))
            continue;

        // Require through the closest (deepest) anchor the module is a descendant of, e.g.
        // `require(Main.X.Y)`; fall through to the standard style computation when there is none.
        if (ctx.config->requireStyle == ImportRequireStyle::NearestAbsolute)
        {
            const ResolvedFixedVariable* bestAnchor = nullptr;
            for (const auto& fixedVariable : resolvedFixedVariables)
            {
                if (!Luau::startsWith(path, fixedVariable.path + "/"))
                    continue;
                if (!bestAnchor || fixedVariable.path.size() > bestAnchor->path.size())
                    bestAnchor = &fixedVariable;
            }

            if (bestAnchor)
            {
                auto remainder = path.substr(bestAnchor->path.size() + 1);
                auto require = convertToScriptPath(bestAnchor->variableName + "/" + remainder);

                size_t anchorMinimum = std::max(minimumLineNumber, bestAnchor->endLine + 1);
                size_t lineNumber = computeBestLineForRequire(*ctx.importsVisitor, *ctx.textDocument, require, anchorMinimum);

                bool prependNewline = ctx.config->separateGroupsWithLine &&
                                      (ctx.importsVisitor->shouldPrependNewline(lineNumber) || lineNumber == bestAnchor->endLine + 1);

                results.emplace_back(InstanceRequireResult{
                    name,
                    path,
                    require,
                    std::nullopt,
                    createRequireTextEdit(name, require, lineNumber, prependNewline, ctx.config->useConst),
                    SortText::AutoImportsAbsolute,
                });
                continue;
            }
        }

        std::string requirePath;
        std::optional<std::pair<std::string, lsp::TextEdit>> serviceEdit;

        // Compute the style of require
        bool isRelative = false;
        auto parent1 = getParentPath(ctx.from), parent2 = getParentPath(path);
        if (ctx.config->requireStyle == ImportRequireStyle::AlwaysRelative ||
            Luau::startsWith(path, "ProjectRoot/") || // All model projects should always require relatively
            (ctx.config->requireStyle != ImportRequireStyle::AlwaysAbsolute &&
                (Luau::startsWith(ctx.from, path) || Luau::startsWith(path, ctx.from) || parent1 == parent2)))
        {
            // HACK: using Uri's purely to access lexicallyRelative
            requirePath = "./" + Uri::file(path).lexicallyRelative(Uri::file(ctx.from));
            isRelative = true;
        }
        else
            requirePath = optimiseAbsoluteRequire(path);

        auto require = convertToScriptPath(requirePath);

        size_t lineNumber = computeBestLineForRequire(*ctx.importsVisitor, *ctx.textDocument, require, minimumLineNumber);

        if (!isRelative)
        {
            // Service will be the first part of the path
            // If we haven't imported the service already, then we auto-import it
            auto service = requirePath.substr(0, requirePath.find('/'));
            if (!contains(ctx.importsVisitor->serviceLineMap, service))
            {
                auto serviceLineNumber = ctx.importsVisitor->findBestLineForService(service, ctx.hotCommentsLineNumber);
                bool appendNewline = false;
                // If there is no firstRequireLine, then the require that we insert will become the first require,
                // so we use `.value_or(serviceLineNumber)` to ensure it equals 0 and a newline is added
                if (ctx.config->separateGroupsWithLine && ctx.importsVisitor->firstRequireLine.value_or(serviceLineNumber) - serviceLineNumber == 0)
                    appendNewline = true;
                serviceEdit = {service, createServiceTextEdit(service, serviceLineNumber, appendNewline, ctx.config->useConst)};
            }
        }

        // Whether we need to add a newline before the require to separate it from the services
        bool prependNewline = ctx.config->separateGroupsWithLine && ctx.importsVisitor->shouldPrependNewline(lineNumber);

        results.emplace_back(InstanceRequireResult{
            name,
            path,
            require,
            serviceEdit,
            createRequireTextEdit(name, require, lineNumber, prependNewline, ctx.config->useConst),
            isRelative ? SortText::AutoImports : SortText::AutoImportsAbsolute,
        });
    }

    return results;
}

void suggestInstanceRequires(const InstanceRequireAutoImporterContext& ctx, std::vector<lsp::CompletionItem>& items)
{
    auto results = computeAllInstanceRequires(ctx);
    for (const auto& [variableName, moduleName, requirePath, serviceEdit, edit, sortText] : results)
    {
        std::vector<lsp::TextEdit> edits;
        if (serviceEdit)
            edits.emplace_back(serviceEdit->second);
        edits.emplace_back(edit);
        items.emplace_back(createSuggestRequire(variableName, edits, sortText, moduleName, requirePath));
    }
}

} // namespace Luau::LanguageServer::AutoImports
