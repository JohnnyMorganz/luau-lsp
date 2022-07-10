#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"

std::vector<lsp::DocumentLink> WorkspaceFolder::documentLink(const lsp::DocumentLinkParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    std::vector<lsp::DocumentLink> result;

    // We need to parse the code, which is currently only done in the type checker
    frontend.check(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule || !sourceModule->root)
        return {};

    // Only resolve document links on require(Foo.Bar.Baz) code
    // TODO: Curerntly we only link at the top level block, not nested blocks
    for (auto stat : sourceModule->root->body)
    {
        if (auto local = stat->as<Luau::AstStatLocal>())
        {
            if (local->values.size == 0)
                continue;

            for (size_t i = 0; i < local->values.size; i++)
            {
                const Luau::AstExprCall* call = local->values.data[i]->as<Luau::AstExprCall>();
                if (!call)
                    continue;

                if (auto maybeRequire = types::matchRequire(*call))
                {
                    if (auto moduleInfo = frontend.moduleResolver.resolveModuleInfo(moduleName, **maybeRequire))
                    {
                        // Resolve the module info to a URI
                        auto realName = fileResolver.resolveToRealPath(moduleInfo->name);
                        if (realName)
                        {
                            lsp::DocumentLink link;
                            link.target = Uri::file(*realName);
                            link.range = lsp::Range{{call->argLocation.begin.line, call->argLocation.begin.column},
                                {call->argLocation.end.line, call->argLocation.end.column - 1}};
                            result.push_back(link);
                        }
                    }
                }
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