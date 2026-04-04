#pragma once
#include "LSP/TextDocument.hpp"
#include "Plugin/SourceMapping.hpp"

namespace Luau::LanguageServer::Plugin
{

// A TextDocument that wraps transformed source with position mapping.
// The base TextDocument holds the TRANSFORMED content (what Luau sees).
// An internal TextDocument holds the ORIGINAL content (what the user sees).
// convertPosition() maps between the two coordinate spaces.
class PluginTextDocument : public TextDocument
{
    TextDocument originalDoc;
    SourceMapping mapping;

public:
    PluginTextDocument(
        lsp::DocumentUri uri,
        std::string languageId,
        size_t version,
        std::string originalContent,
        std::string transformedContent,
        SourceMapping mapping);

    // LSP position (in original) -> Luau position (in transformed)
    Luau::Position convertPosition(const lsp::Position& position) const override;

    // Luau position (in transformed) -> LSP position (in original)
    lsp::Position convertPosition(const Luau::Position& position) const override;

    // Access to mapping for special cases
    const SourceMapping& getMapping() const
    {
        return mapping;
    }

    // Access to original content if needed
    std::string getOriginalText() const;
};

} // namespace Luau::LanguageServer::Plugin
