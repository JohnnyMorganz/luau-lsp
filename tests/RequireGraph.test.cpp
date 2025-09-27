#include "doctest.h"
#include "Fixture.h"

TEST_SUITE_BEGIN("RequireGraph");

struct DotFileOutput
{
    std::unordered_set<std::string_view> nodes;
    std::vector<std::pair<std::string_view, std::string_view>> edges;

    bool containsNode(const std::string& node) const
    {
        return nodes.find(node) != nodes.end();
    }

    bool containsEdge(const std::string& from, const std::string& to) const
    {
        for (const auto& edge : edges)
        {
            if (from == edge.first && to == edge.second)
                return true;
        }
        return false;
    }
};

DotFileOutput parseDotResult(const std::string& text)
{
    auto lines = Luau::split(text, '\n');
    REQUIRE(lines.size() > 2);
    REQUIRE_EQ(lines[0], "digraph luau_require_graph {");
    REQUIRE_EQ(lines[lines.size() - 1], "}");

    DotFileOutput result;

    std::vector<std::string_view> edgeLines;
    std::unordered_map<std::string_view, std::string_view> nodeIdMap;

    auto ARROW = std::string(" -> ");
    for (auto i = 1; i < lines.size() - 1; ++i)
    {
        auto line = lines[i];
        if (line.find(ARROW) != std::string::npos)
        {
            // Defer edge lines until we know the node mappings
            edgeLines.push_back(line);
        }
        else
        {
            REQUIRE(line.size() > 2);
            REQUIRE(line[0] == 'N');
            auto attributesStart = line.find('[');
            REQUIRE(attributesStart != std::string::npos);
            auto nodeId = line.substr(0, attributesStart);

            auto labelStart = line.find('"');
            REQUIRE(labelStart != std::string::npos);
            auto labelEnd = line.find('"', labelStart + 1);
            REQUIRE(labelEnd != std::string::npos);
            auto label = line.substr(labelStart + 1, labelEnd - labelStart - 1);
            result.nodes.insert(label);
            nodeIdMap.insert({nodeId, label});
        }
    }

    for (const auto& line : edgeLines)
    {
        auto arrow = line.find(ARROW);
        REQUIRE(arrow != std::string::npos);

        auto firstNode = line.substr(0, arrow);
        auto secondNode = line.substr(arrow + ARROW.size(), line.size() - ARROW.size() - arrow - 1);

        REQUIRE(nodeIdMap.find(firstNode) != nodeIdMap.end());
        REQUIRE(nodeIdMap.find(secondNode) != nodeIdMap.end());
        result.edges.emplace_back(nodeIdMap.find(firstNode)->second, nodeIdMap.find(secondNode)->second);
    }

    return result;
}

TEST_CASE_FIXTURE(Fixture, "simple_require_graph")
{
    auto firstDependency = newDocument("firstDependency.luau", R"(
        return {}
    )");

    auto secondDependency = newDocument("secondDependency.luau", R"(
        return {}
    )");

    auto uri = newDocument("base.luau", R"(
        local firstDependency = require("firstDependency.luau")
        local secondDependency = require("secondDependency.luau")
    )");

    // Index dependencies
    workspace.frontend.parse(workspace.fileResolver.getModuleName(uri));

    lsp::RequireGraphParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.fromTextDocumentOnly = true;

    auto result = workspace.requireGraph(params);
    auto graph = parseDotResult(result);

    REQUIRE_EQ(graph.nodes.size(), 3);
    REQUIRE_EQ(graph.edges.size(), 2);
    REQUIRE(graph.containsNode(uri.fsPath()));
    REQUIRE(graph.containsNode(firstDependency.fsPath()));
    REQUIRE(graph.containsNode(secondDependency.fsPath()));
    REQUIRE(graph.containsEdge(uri.fsPath(), firstDependency.fsPath()));
    REQUIRE(graph.containsEdge(uri.fsPath(), secondDependency.fsPath()));
}

TEST_CASE_FIXTURE(Fixture, "full_require_graph_with_disconnected_module")
{
    auto firstDependency = newDocument("firstDependency.luau", R"(
        return {}
    )");

    auto secondDependency = newDocument("secondDependency.luau", R"(
        return {}
    )");

    auto disconnectedModule = newDocument("disconnected.luau", R"(
        return {}
    )");

    auto uri = newDocument("base.luau", R"(
        local firstDependency = require("firstDependency.luau")
        local secondDependency = require("secondDependency.luau")
    )");

    // Index dependencies
    workspace.frontend.parse(workspace.fileResolver.getModuleName(uri));
    workspace.frontend.parse(workspace.fileResolver.getModuleName(disconnectedModule));

    lsp::RequireGraphParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.fromTextDocumentOnly = false;

    auto result = workspace.requireGraph(params);
    auto graph = parseDotResult(result);

    REQUIRE_EQ(graph.nodes.size(), 4);
    REQUIRE_EQ(graph.edges.size(), 2);
    REQUIRE(graph.containsNode(uri.fsPath()));
    REQUIRE(graph.containsNode(firstDependency.fsPath()));
    REQUIRE(graph.containsNode(secondDependency.fsPath()));
    REQUIRE(graph.containsNode(disconnectedModule.fsPath()));
    REQUIRE(graph.containsEdge(uri.fsPath(), firstDependency.fsPath()));
    REQUIRE(graph.containsEdge(uri.fsPath(), secondDependency.fsPath()));
}

TEST_SUITE_END();
