#include "LSP/LanguageServer.hpp"
#include "LSP/Sourcemap.hpp"
#include "LSP/PluginDataModel.hpp"

void LanguageServer::onStudioPluginFullChange(const PluginNode& dataModel)
{
    client->sendLogMessage(lsp::MessageType::Info, "received full change from studio plugin");

    // TODO: handle multi-workspace setup
    auto workspace = workspaceFolders.at(0);
    workspace->fileResolver.pluginInfo = std::make_shared<PluginNode>(dataModel);

    // Mutate the sourcemap with the new information
    workspace->updateSourceMap();
}

void LanguageServer::onStudioPluginClear()
{
    client->sendLogMessage(lsp::MessageType::Info, "received clear from studio plugin");

    // TODO: handle multi-workspace setup
    auto workspace = workspaceFolders.at(0);

    workspace->fileResolver.pluginInfo = nullptr;

    // Mutate the sourcemap with the new information
    workspace->updateSourceMap();
}

void SourceNode::mutateWithPluginInfo(const PluginNodePtr& pluginInstance)
{
    // We currently perform purely additive changes where we add in new children
    for (const auto& dmChild : pluginInstance->children)
    {
        if (auto existingChildNode = findChild(dmChild->name))
        {
            existingChildNode.value()->mutateWithPluginInfo(dmChild);
        }
        else
        {
            SourceNode childNode;
            childNode.name = dmChild->name;
            childNode.className = dmChild->className;
            childNode.mutateWithPluginInfo(dmChild);

            children.push_back(std::make_shared<SourceNode>(childNode));
        }
    }
}