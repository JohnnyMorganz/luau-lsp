#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"
#include "LSP/LuauExt.hpp"

struct RequireInfo
{
    Luau::AstExpr* require = nullptr;
    Luau::Location location;
};

struct FindRequireVisitor : public Luau::AstVisitor
{
    std::vector<RequireInfo> requireInfos{};

    bool visit(Luau::AstExprCall* call) override
    {
        if (auto maybeRequire = types::matchRequire(*call))
            requireInfos.emplace_back(RequireInfo{*maybeRequire, call->argLocation});
        return true;
    }

    bool visit(Luau::AstStatBlock* block) override
    {
        for (Luau::AstStat* stat : block->body)
        {
            stat->visit(this);
        }

        return false;
    }
};

std::vector<lsp::DocumentLink> WorkspaceFolder::documentLink(const lsp::DocumentLinkParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    std::vector<lsp::DocumentLink> result{};

    // We need to parse the code, which is currently only done in the type checker
    frontend.check(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule || !sourceModule->root)
        return {};

    FindRequireVisitor visitor;
    visitor.visit(sourceModule->root);

    for (auto& require : visitor.requireInfos)
    {
        if (auto moduleInfo = frontend.moduleResolver.resolveModuleInfo(moduleName, *require.require))
        {
            // Resolve the module info to a URI
            auto realName = fileResolver.resolveToRealPath(moduleInfo->name);
            if (realName)
            {
                lsp::DocumentLink link;
                link.target = Uri::file(*realName);
                link.range = lsp::Range{
                    {require.location.begin.line, require.location.begin.column}, {require.location.end.line, require.location.end.column - 1}};
                result.push_back(link);
            }
        }
    }

    return result;
}

std::vector<lsp::DocumentLink> LanguageServer::documentLink(const lsp::DocumentLinkParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->documentLink(params);
}