#include "Platform/InstanceRequireAutoImporter.hpp"

#include "LSP/Completion.hpp"
#include "Platform/RobloxPlatform.hpp"

namespace Luau::LanguageServer::AutoImports
{

lsp::TextEdit createServiceTextEdit(const std::string& name, size_t lineNumber, bool appendNewline)
{
    auto range = lsp::Range{{lineNumber, 0}, {lineNumber, 0}};
    auto importText = "local " + name + " = game:GetService(\"" + name + "\")\n";
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

std::vector<InstanceRequireResult> computeAllInstanceRequires(const InstanceRequireAutoImporterContext& ctx)
{
    std::vector<InstanceRequireResult> results;
    size_t minimumLineNumber = computeMinimumLineNumberForRequire(*ctx.importsVisitor, ctx.hotCommentsLineNumber);

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
                serviceEdit = {service, createServiceTextEdit(service, serviceLineNumber, appendNewline)};
            }
        }

        // Whether we need to add a newline before the require to separate it from the services
        bool prependNewline = ctx.config->separateGroupsWithLine && ctx.importsVisitor->shouldPrependNewline(lineNumber);

        results.emplace_back(InstanceRequireResult{
            name,
            path,
            require,
            serviceEdit,
            createRequireTextEdit(name, require, lineNumber, prependNewline),
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
