#pragma once
#include <optional>
#include <filesystem>
#include <unordered_map>
#include "Luau/FileResolver.h"
#include "Luau/StringUtils.h"
#include "Luau/Config.h"
#include "LSP/Uri.hpp"
#include "LSP/Protocol.hpp"
#include "LSP/Sourcemap.hpp"
#include "LSP/TextDocument.hpp"

struct WorkspaceFileResolver
    : Luau::FileResolver
    , Luau::ConfigResolver
{
    Luau::Config defaultConfig;

    // The root source node from a parsed Rojo source map
    lsp::DocumentUri rootUri;
    SourceNodePtr rootSourceNode;
    mutable std::unordered_map<std::string, SourceNodePtr> realPathsToSourceNodes;
    mutable std::unordered_map<Luau::ModuleName, SourceNodePtr> virtualPathsToSourceNodes;

    // Currently opened files where content is managed by client
    mutable std::unordered_map<Luau::ModuleName, TextDocument> managedFiles;
    mutable std::unordered_map<std::string, Luau::Config> configCache;
    mutable std::vector<std::pair<std::filesystem::path, std::string>> configErrors;

    WorkspaceFileResolver()
    {
        defaultConfig.mode = Luau::Mode::Nonstrict;
    }

    /// The file is managed by the client, so FS will be out of date
    bool isManagedFile(const Luau::ModuleName& name) const
    {
        return managedFiles.find(name) != managedFiles.end();
    }

    /// The name points to a virtual path (i.e., game/ or ProjectRoot/)
    bool isVirtualPath(const Luau::ModuleName& name) const
    {
        return name == "game" || name == "ProjectRoot" || Luau::startsWith(name, "game/") || Luau::startsWith(name, "ProjectRoot");
    }

    // Return the corresponding module name from a file Uri
    // We first try and find a virtual file path which matches it, and return that. Otherwise, we use the file system path
    Luau::ModuleName getModuleName(const Uri& name);

    std::optional<SourceNodePtr> getSourceNodeFromVirtualPath(const Luau::ModuleName& name) const;

    std::optional<SourceNodePtr> getSourceNodeFromRealPath(const std::string& name) const;

    std::optional<std::filesystem::path> getRealPathFromSourceNode(const SourceNodePtr& sourceNode) const;
    Luau::ModuleName getVirtualPathFromSourceNode(const SourceNodePtr& sourceNode) const;

    std::optional<Luau::ModuleName> resolveToVirtualPath(const std::string& name) const;

    std::optional<std::filesystem::path> resolveToRealPath(const Luau::ModuleName& name) const;

    std::optional<Luau::SourceCode> readSource(const Luau::ModuleName& name) override;

    std::optional<Luau::ModuleInfo> resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* node) override;

    std::string getHumanReadableModuleName(const Luau::ModuleName& name) const override;

    const Luau::Config& getConfig(const Luau::ModuleName& name) const override;

    const Luau::Config& readConfigRec(const std::filesystem::path& path) const;

    void clearConfigCache();

    void writePathsToMap(const SourceNodePtr& node, const std::string& base);

    void updateSourceMap(const std::string& sourceMapContents);
};