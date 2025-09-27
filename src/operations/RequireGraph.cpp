#include "LSP/Workspace.hpp"
#include "Luau/BytecodeBuilder.h"
#include "Luau/Set.h"

#include <queue>

lsp::RequireGraphResult WorkspaceFolder::requireGraph(const lsp::RequireGraphParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());

    std::string result;
    Luau::formatAppend(result, "digraph luau_require_graph {\n");

    Luau::DenseHashMap<Luau::ModuleName, int> nodeIdMap{{}};

    if (params.fromTextDocumentOnly)
    {
        // Reindex file to ensure it's up to date
        frontend.parse(moduleName);

        // Breadth-first search from current node for its requires
        Luau::Set<Luau::ModuleName> seenSet{{}};
        std::queue<Luau::ModuleName> queue;

        int currentNodeId = 0;

        queue.push(moduleName);

        while (!queue.empty())
        {
            Luau::ModuleName currentModuleName = queue.front();
            queue.pop();

            if (seenSet.contains(currentModuleName))
                continue;
            seenSet.insert(currentModuleName);

            if (!nodeIdMap.contains(currentModuleName))
                nodeIdMap[currentModuleName] = currentNodeId++;

            if (auto node = frontend.sourceNodes.find(currentModuleName); node != frontend.sourceNodes.end())
            {
                for (const auto& [requiredModuleName, _] : node->second->requireLocations)
                {
                    if (!nodeIdMap.contains(requiredModuleName))
                        nodeIdMap[requiredModuleName] = currentNodeId++;

                    auto fromNodeId = nodeIdMap[currentModuleName];
                    auto toNodeId = nodeIdMap[requiredModuleName];

                    Luau::formatAppend(result, "N%d -> N%d;\n", fromNodeId, toNodeId);
                    queue.push(requiredModuleName);
                }
            }
        }
    }
    else
    {
        int currentNodeId = 0;
        for (const auto& [currentModuleName, sourceNode] : frontend.sourceNodes)
        {
            if (!nodeIdMap.contains(currentModuleName))
                nodeIdMap[currentModuleName] = currentNodeId++;
            for (const auto& [requiredModuleName, _] : sourceNode->requireLocations)
            {
                if (!nodeIdMap.contains(requiredModuleName))
                    nodeIdMap[requiredModuleName] = currentNodeId++;

                auto fromNodeId = nodeIdMap[currentModuleName];
                auto toNodeId = nodeIdMap[requiredModuleName];

                Luau::formatAppend(result, "N%d -> N%d;\n", fromNodeId, toNodeId);
            }
        }
    }

    for (const auto& [nodeModuleName, nodeId] : nodeIdMap)
        Luau::formatAppend(result, "N%d[label=\"%s\"][shape=\"box\"];\n", nodeId, fileResolver.getHumanReadableModuleName(nodeModuleName).c_str());

    Luau::formatAppend(result, "}\n");
    return result;
}