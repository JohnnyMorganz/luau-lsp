#include "Plugin/PluginTextDocument.hpp"

namespace Luau::LanguageServer::Plugin
{

PluginTextDocument::PluginTextDocument(
    lsp::DocumentUri uri,
    std::string languageId,
    size_t version,
    std::string originalContent,
    std::string transformedContent,
    SourceMapping mapping)
    : TextDocument(uri, languageId, version, std::move(transformedContent))
    , originalDoc(std::move(uri), std::move(languageId), version, std::move(originalContent))
    , mapping(std::move(mapping))
{
}

Luau::Position PluginTextDocument::convertPosition(const lsp::Position& position) const
{
    // Convert LSP position (UTF-16) to Luau position using ORIGINAL content
    Luau::Position originalPos = originalDoc.convertPosition(position);

    // Then map from original to transformed position
    if (auto transformedPos = mapping.originalToTransformed(originalPos))
        return *transformedPos;

    return originalPos;
}

lsp::Position PluginTextDocument::convertPosition(const Luau::Position& position) const
{
    // Map from transformed to original position
    Luau::Position originalPos = position;
    if (auto mapped = mapping.transformedToOriginal(position))
        originalPos = *mapped;

    // Convert original Luau position to LSP position using ORIGINAL content
    return originalDoc.convertPosition(originalPos);
}

std::string PluginTextDocument::getOriginalText() const
{
    return originalDoc.getText();
}

} // namespace Luau::LanguageServer::Plugin
