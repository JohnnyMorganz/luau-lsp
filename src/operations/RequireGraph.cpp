#include "LSP/RequireGraph.hpp"
#include "LuauFileUtils.hpp"
#include "Analyze/AnalyzeCli.hpp"
#include "LSP/Workspace.hpp"
#include "Luau/BytecodeBuilder.h"
#include "Luau/Set.h"
#include "Platform/RobloxPlatform.hpp"
#include "argparse/argparse.hpp"

#include <queue>

struct RequireGraph
{
    std::vector<Luau::ModuleName> nodes;
    std::vector<std::pair<Luau::ModuleName, Luau::ModuleName>> edges;
};

RequireGraph computeFullRequireGraph(const Luau::Frontend& frontend)
{
    RequireGraph result;
    result.nodes.reserve(frontend.sourceNodes.size());

    for (const auto& [currentModuleName, sourceNode] : frontend.sourceNodes)
    {
        result.nodes.emplace_back(currentModuleName);
        result.edges.reserve(result.edges.size() + sourceNode->requireLocations.size());
        for (const auto& [requiredModuleName, _] : sourceNode->requireLocations)
            result.edges.emplace_back(currentModuleName, requiredModuleName);
    }

    return result;
}

RequireGraph computeRequireGraphFromRoot(const Luau::Frontend& frontend, const Luau::ModuleName& root)
{
    RequireGraph result;

    // Breadth-first search from current node for its requires
    Luau::Set<Luau::ModuleName> seenSet{{}};
    std::queue<Luau::ModuleName> queue;

    queue.push(root);

    while (!queue.empty())
    {
        Luau::ModuleName currentModuleName = queue.front();
        queue.pop();

        if (seenSet.contains(currentModuleName))
            continue;
        seenSet.insert(currentModuleName);
        result.nodes.emplace_back(currentModuleName);

        if (auto node = frontend.sourceNodes.find(currentModuleName); node != frontend.sourceNodes.end())
        {
            for (const auto& [requiredModuleName, _] : node->second->requireLocations)
            {
                result.edges.emplace_back(currentModuleName, requiredModuleName);
                queue.push(requiredModuleName);
            }
        }
    }

    return result;
}

struct ChunkedWriter
{
    static constexpr int CHUNK_SIZE = 1024;
    std::vector<std::string> chunks;

    ChunkedWriter()
    {
        newChunk();
    }

    [[nodiscard]] std::string str() const
    {
        return Luau::join(chunks, "");
    }

    void newChunk()
    {
        chunks.emplace_back();
        chunks.back().reserve(CHUNK_SIZE);
    }

    void appendChunk(std::string_view sv)
    {
        if (sv.size() > CHUNK_SIZE)
        {
            chunks.emplace_back(sv);
            newChunk();
            return;
        }

        auto& chunk = chunks.back();
        if (chunk.size() + sv.size() < CHUNK_SIZE)
        {
            chunk.append(sv.data(), sv.size());
            return;
        }

        size_t prefix = CHUNK_SIZE - chunk.size();
        chunk.append(sv.data(), prefix);
        newChunk();

        chunks.back().append(sv.data() + prefix, sv.size() - prefix);
    }

    void writeRaw(const std::string_view sv)
    {
        appendChunk(sv);
    }

    void writeRaw(char c)
    {
        appendChunk({&c, 1});
    }

    void writeEscapedString(const std::string_view sv)
    {
        writeRaw("\"");

        for (char c : sv)
        {
            if (c == '"')
                writeRaw("\\\"");
            else if (c == '\\')
                writeRaw("\\\\");
            else if (c < ' ')
                writeRaw(Luau::format("\\u%04x", c));
            else if (c == '\n')
                writeRaw("\\n");
            else
                writeRaw(c);
        }

        writeRaw("\"");
    }
};

std::string requireGraphToDot(const RequireGraph& requireGraph, const Luau::FileResolver& fileResolver)
{
    Luau::DenseHashMap<Luau::ModuleName, int> nodeIdMap{{}};

    ChunkedWriter writer;
    writer.writeRaw("digraph luau_require_graph {\n");

    int currentNodeId = 0;
    for (const auto& moduleName : requireGraph.nodes)
    {
        auto nodeId = currentNodeId++;
        nodeIdMap[moduleName] = nodeId;
        writer.writeRaw(Luau::format("N%d[label=\"%s\"][shape=\"box\"];\n", nodeId, fileResolver.getHumanReadableModuleName(moduleName).c_str()));
    }

    for (const auto& [fromModuleName, toModuleName] : requireGraph.edges)
    {
        auto fromNodeId = nodeIdMap[fromModuleName];
        auto toNodeId = nodeIdMap[toModuleName];

        writer.writeRaw(Luau::format("N%d -> N%d;\n", fromNodeId, toNodeId));
    }

    writer.writeRaw("}\n");
    return writer.str();
}

std::string requireGraphToJson(const RequireGraph& requireGraph, const Luau::FileResolver& fileResolver)
{
    ChunkedWriter writer;
    writer.writeRaw(R"({"nodes":[)");
    bool first = true;
    for (const auto& node : requireGraph.nodes)
    {
        if (!first)
            writer.writeRaw(',');
        writer.writeEscapedString(fileResolver.getHumanReadableModuleName(fileResolver.getHumanReadableModuleName(node)));
        first = false;
    }
    writer.writeRaw(R"(],"edges":[)");
    first = true;
    for (const auto& [from, to] : requireGraph.edges)
    {
        if (!first)
            writer.writeRaw(',');
        writer.writeRaw('[');
        writer.writeEscapedString(fileResolver.getHumanReadableModuleName(fileResolver.getHumanReadableModuleName(from)));
        writer.writeRaw(',');
        writer.writeEscapedString(fileResolver.getHumanReadableModuleName(to));
        writer.writeRaw(']');
        first = false;
    }
    writer.writeRaw("]}\n");
    return writer.str();
}

lsp::RequireGraphResult WorkspaceFolder::requireGraph(const lsp::RequireGraphParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());

    if (params.fromTextDocumentOnly)
    {
        // Reindex file to ensure it's up to date
        frontend.parse(moduleName);
        auto requireGraph = computeRequireGraphFromRoot(frontend, moduleName);
        return requireGraphToDot(requireGraph, fileResolver);
    }
    else
    {
        auto requireGraph = computeFullRequireGraph(frontend);
        return requireGraphToDot(requireGraph, fileResolver);
    }
}

int startRequireGraph(const argparse::ArgumentParser& program)
{
    auto sourcemapPath = program.present<std::string>("--sourcemap");
    auto baseLuaurc = program.present<std::string>("--base-luaurc");
    auto outputFormat = program.get<std::string>("--output-format");
    std::vector<std::string> files{};

    CliClient client;

    auto currentWorkingDirectory = Luau::FileUtils::getCurrentWorkingDirectory();
    if (!currentWorkingDirectory)
    {
        fprintf(stderr, "Failed to determine current working directory\n");
        return 1;
    }

    if (auto filesArg = program.present<std::vector<std::string>>("files"))
        files = getFilesToAnalyze(*filesArg, {});

    if (files.empty())
    {
        fprintf(stderr, "error: no files provided\n");
        return 1;
    }

    Luau::FrontendOptions frontendOptions;
    frontendOptions.retainFullTypeGraphs = false;
    frontendOptions.runLintChecks = false;

    WorkspaceFileResolver fileResolver;
    if (baseLuaurc)
    {
        if (std::optional<std::string> contents = Luau::FileUtils::readFile(*baseLuaurc))
        {
            Luau::Config result;
            std::optional<std::string> error = WorkspaceFileResolver::parseConfig(Uri::file(*baseLuaurc), *contents, result);
            if (error)
            {
                fprintf(stderr, "%s: %s\n", baseLuaurc->c_str(), error->c_str());
                return 1;
            }
            fileResolver.defaultConfig = std::move(result);
        }
        else
        {
            fprintf(stderr, "Failed to read base .luaurc configuration at '%s'\n", baseLuaurc->c_str());
            return 1;
        }
    }

    fileResolver.rootUri = Uri::file(*currentWorkingDirectory);
    fileResolver.client = &client;

    if (auto platformArg = program.present("--platform"))
    {
        if (platformArg == "standard")
            client.configuration.platform.type = LSPPlatformConfig::Standard;
        else if (platformArg == "roblox")
            client.configuration.platform.type = LSPPlatformConfig::Roblox;
    }

    std::unique_ptr<LSPPlatform> platform = LSPPlatform::getPlatform(client.configuration, &fileResolver);

    fileResolver.platform = platform.get();
    fileResolver.requireSuggester = fileResolver.platform->getRequireSuggester();

    Luau::Frontend frontend(&fileResolver, &fileResolver, frontendOptions);
    Luau::freeze(frontend.globals.globalTypes);

    if (sourcemapPath)
    {
        if (client.configuration.platform.type == LSPPlatformConfig::Roblox)
        {
            auto robloxPlatform = dynamic_cast<RobloxPlatform*>(platform.get());

            if (auto sourceMapContents = Luau::FileUtils::readFile(*sourcemapPath))
            {
                robloxPlatform->updateSourceNodeMap(sourceMapContents.value());
                // We don't need to call 'handleSourcemapUpdate' as we are not type checking
            }
            else
            {
                fprintf(stderr, "Cannot load sourcemap path %s: failed to read contents\n", sourcemapPath->c_str());
                return 1;
            }
        }
        else
        {
            std::cerr << "warning: a sourcemap was provided, but the current platform is not `roblox`. Use `--platform roblox` to ensure the "
                         "sourcemap option is respected.\n";
        }
    }

    std::vector<Luau::ModuleName> moduleNames;
    moduleNames.reserve(files.size());
    for (const auto& file : files)
        moduleNames.emplace_back(fileResolver.getModuleName(Uri::file(file)));

    frontend.parseModules(moduleNames);

    auto graph = computeFullRequireGraph(frontend);
    if (outputFormat == "dot")
        std::cout << requireGraphToDot(graph, fileResolver);
    else
        std::cout << requireGraphToJson(graph, fileResolver);

    return 0;
}