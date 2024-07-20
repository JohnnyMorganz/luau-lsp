#include <Platform/RobloxPlatform.hpp>

#include <LSP/Workspace.hpp>

void RobloxPlatform::onDidChangeWatchedFiles(const lsp::FileEvent& change)
{
    auto filePath = change.uri.fsPath();
    if (filePath.filename() == "sourcemap.json")
    {
        workspaceFolder->client->sendLogMessage(lsp::MessageType::Info, "Registering sourcemap changed for workspace " + workspaceFolder->name);
        updateSourceMap();
    }
}

void RobloxPlatform::setupWithConfiguration(const ClientConfiguration& config)
{
    if (config.sourcemap.enabled)
    {
        workspaceFolder->client->sendTrace("workspace: sourcemap enabled");
        if (!workspaceFolder->isNullWorkspace() && !updateSourceMap())
        {
            workspaceFolder->client->sendWindowMessage(lsp::MessageType::Error,
                "Failed to load sourcemap.json for workspace '" + workspaceFolder->name + "'. Instance information will not be available");
        }
    }

    applyStrictDataModelTypesConfiguration(config.diagnostics.strictDatamodelTypes);
}

// Prepare module scope so that we can dynamically reassign the type of "script" to retrieve instance info
// TODO: expressiveTypes is used because of a Luau issue where we can't cast a most specific Instance type (which we create here)
// to another type. For the time being, we therefore make all our DataModel instance types marked as "any".
// Remove this once Luau has improved
void RobloxPlatform::applyStrictDataModelTypesConfiguration(bool expressiveTypes)
{
    workspaceFolder->frontend.prepareModuleScope = [this, expressiveTypes](
                                                       const Luau::ModuleName& name, const Luau::ScopePtr& scope, bool forAutocomplete)
    {
        Luau::GlobalTypes& globals = forAutocomplete ? workspaceFolder->frontend.globalsForAutocomplete : workspaceFolder->frontend.globals;

        // TODO: we hope to remove these in future!
        if (!expressiveTypes && !forAutocomplete)
        {
            scope->bindings[Luau::AstName("script")] = Luau::Binding{globals.builtinTypes->anyType};
            scope->bindings[Luau::AstName("workspace")] = Luau::Binding{globals.builtinTypes->anyType};
            scope->bindings[Luau::AstName("game")] = Luau::Binding{globals.builtinTypes->anyType};
        }

        if (expressiveTypes || forAutocomplete)
            if (auto node = isVirtualPath(name) ? getSourceNodeFromVirtualPath(name) : getSourceNodeFromRealPath(name))
                scope->bindings[Luau::AstName("script")] = Luau::Binding{getSourcemapType(globals, instanceTypes, node.value())};
    };
}
