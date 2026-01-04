#include "LSP/Workspace.hpp"
#include "LSP/LanguageServer.hpp"
#include "LSP/LuauExt.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "LuauFileUtils.hpp"

#include <unordered_set>

namespace
{
bool isLuauFile(const Uri& uri)
{
    auto path = uri.fsPath();
    return endsWith(path, ".lua") || endsWith(path, ".luau");
}

std::vector<lsp::FileRename> expandFolderRenames(const std::vector<lsp::FileRename>& renames)
{
    std::vector<lsp::FileRename> expanded;
    std::unordered_set<Uri, UriHash> seen;

    for (const auto& rename : renames)
    {
        if (isLuauFile(rename.oldUri))
        {
            if (seen.insert(rename.oldUri).second)
                expanded.push_back(rename);
            continue;
        }

        if (!rename.oldUri.isDirectory())
            continue;

        // It's a folder - find all Luau files within it
        auto oldFolderPath = rename.oldUri.fsPath();
        auto newFolderPath = rename.newUri.fsPath();

        // Ensure paths end with separator for proper prefix replacement
        if (!oldFolderPath.empty() && oldFolderPath.back() != '/')
            oldFolderPath += '/';
        if (!newFolderPath.empty() && newFolderPath.back() != '/')
            newFolderPath += '/';

        Luau::FileUtils::traverseDirectoryRecursive(rename.oldUri.fsPath(),
            [&](const std::string& filePath)
            {
                auto fileUri = Uri::file(filePath);
                if (!isLuauFile(fileUri))
                    return;

                if (!seen.insert(fileUri).second)
                    return; // Already processed (deduplication)

                // Compute new path by replacing old folder prefix with new folder prefix
                auto relativePath = filePath.substr(oldFolderPath.length() - 1); // Keep the leading /
                auto newFilePath = newFolderPath.substr(0, newFolderPath.length() - 1) + relativePath;

                expanded.push_back(lsp::FileRename{fileUri, Uri::file(newFilePath)});
            });
    }

    return expanded;
}
} // namespace

lsp::WorkspaceEdit WorkspaceFolder::onWillRenameFiles(const std::vector<lsp::FileRename>& renames)
{
    lsp::WorkspaceEdit result;

    ClientConfiguration config = client->getConfiguration(rootUri);

    if (config.updateRequiresOnFileMove.enabled == UpdateRequiresOnFileMoveConfig::Never)
        return result;

    // Expand folder renames into individual file renames
    auto expandedRenames = expandFolderRenames(renames);

    for (const auto& rename : expandedRenames)
    {
        auto oldModuleName = fileResolver.getModuleName(rename.oldUri);
        auto sourceNode = frontend.sourceNodes.find(oldModuleName);
        if (sourceNode == frontend.sourceNodes.end())
            continue;

        auto newModuleName = platform->inferModuleNameFromUri(rename.newUri);
        if (!newModuleName)
        {
            client->sendWindowMessage(
                lsp::MessageType::Warning, "Failed to determine new location for '" + rename.newUri.toString() + "', requires are not updated");
            continue;
        }

        for (const auto& dependentModule : sourceNode->second->dependents)
        {
            if (dependentModule == oldModuleName)
                continue;

            frontend.parse(dependentModule);
            if (auto sourceModule = frontend.getSourceModule(dependentModule); !sourceModule || !sourceModule->root)
                continue;

            if (frontend.requireTrace.find(dependentModule) == frontend.requireTrace.end())
                continue;

            auto& require = frontend.requireTrace[dependentModule];

            auto dependentUri = fileResolver.getUri(dependentModule);
            auto textDocument = fileResolver.getTextDocumentFromModuleName(dependentModule);
            if (!textDocument)
                continue;

            for (const auto& [node, moduleInfo] : require.exprs)
            {
                if (moduleInfo.name != oldModuleName)
                    continue;

                // Get the require call's argument location
                auto* callExpr = node->as<Luau::AstExprCall>();
                if (!callExpr || callExpr->args.size == 0)
                    continue;

                auto* argExpr = callExpr->args.data[0];

                // Compute new require path using platform (with module names)
                auto newPath = platform->computeNewRequirePath(dependentModule, *newModuleName, node, config);
                if (!newPath)
                    continue;

                // Create the text edit to replace the require argument
                lsp::TextEdit edit;
                edit.range = textDocument->convertLocation(argExpr->location);
                edit.newText = *newPath;

                result.changes[dependentUri].push_back(edit);
            }
        }
    }

    return result;
}

lsp::WorkspaceEdit LanguageServer::onWillRenameFiles(const lsp::RenameFilesParams& params)
{
    // Group renames by workspace
    std::unordered_map<WorkspaceFolderPtr, std::vector<lsp::FileRename>> workspaceRenames;

    for (const auto& rename : params.files)
    {
        auto workspace = findWorkspace(rename.oldUri);
        workspaceRenames[workspace].push_back(rename);
    }

    // Process each workspace's renames
    lsp::WorkspaceEdit combinedEdit;
    for (const auto& [workspace, renames] : workspaceRenames)
    {
        auto edit = workspace->onWillRenameFiles(renames);
        for (const auto& [uri, edits] : edit.changes)
        {
            combinedEdit.changes[uri].insert(combinedEdit.changes[uri].end(), edits.begin(), edits.end());
        }
    }

    return combinedEdit;
}
