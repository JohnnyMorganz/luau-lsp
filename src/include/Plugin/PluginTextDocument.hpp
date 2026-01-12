#pragma once
#include "LSP/TextDocument.hpp"
#include "Plugin/SourceMapping.hpp"

namespace Luau::LanguageServer::Plugin
{

// A TextDocument that wraps transformed source with position mapping.
// getText() returns transformed content, convertPosition() maps between original and transformed.
class PluginTextDocument : public TextDocument
{
    SourceMapping mapping;
    std::string transformedContent;
    mutable std::optional<std::vector<size_t>> _transformedLineOffsets;
    mutable std::optional<std::vector<size_t>> _originalLineOffsets;

    // Helper to get line offsets for original content (cached)
    const std::vector<size_t>& getOriginalLineOffsets() const;

public:
    PluginTextDocument(
        lsp::DocumentUri uri,
        std::string languageId,
        size_t version,
        std::string originalContent,
        std::string transformedContent,
        SourceMapping mapping);

    // Override to return TRANSFORMED content (what Luau type checker sees)
    std::string getText(std::optional<lsp::Range> range = std::nullopt) const override;
    std::string getLine(size_t index) const override;

    // LSP position (in original) -> offset in transformed content
    size_t offsetAt(const lsp::Position& position) const override;

    // Offset in transformed content -> LSP position (in original)
    lsp::Position positionAt(size_t offset) const override;

    // LSP position (in original) -> Luau position (in transformed)
    Luau::Position convertPosition(const lsp::Position& position) const override;

    // Luau position (in transformed) -> LSP position (in original)
    lsp::Position convertPosition(const Luau::Position& position) const override;

    // Line offsets for transformed content
    const std::vector<size_t>& getLineOffsets() const override;
    size_t lineCount() const override;

    // Access to mapping for special cases
    const SourceMapping& getMapping() const
    {
        return mapping;
    }

    // Access to original content if needed
    std::string getOriginalText() const;
};

} // namespace Luau::LanguageServer::Plugin
