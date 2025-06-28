#pragma once
#include <optional>
#include <string>
#include "Protocol/Structures.hpp"

namespace lsp
{
struct DidOpenTextDocumentParams
{
    TextDocumentItem textDocument;
};
NLOHMANN_DEFINE_OPTIONAL(DidOpenTextDocumentParams, textDocument)

struct TextDocumentContentChangeEvent
{
    // If only text is provided, then it's considered to be the whole document
    std::optional<Range> range = std::nullopt;
    std::string text;
};
NLOHMANN_DEFINE_OPTIONAL(TextDocumentContentChangeEvent, range, text)

struct DidChangeTextDocumentParams
{
    VersionedTextDocumentIdentifier textDocument;
    std::vector<TextDocumentContentChangeEvent> contentChanges;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DidChangeTextDocumentParams, textDocument, contentChanges)

struct DidSaveTextDocumentParams
{
    /**
     * The document that was saved.
     */
    TextDocumentIdentifier textDocument;
    /**
     * Optional the content when saved. Depends on the includeText value
     * when the save notification was requested.
     */
    std::optional<std::string> text = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(DidSaveTextDocumentParams, textDocument, text)

struct DidCloseTextDocumentParams
{
    TextDocumentIdentifier textDocument;
};
NLOHMANN_DEFINE_OPTIONAL(DidCloseTextDocumentParams, textDocument)
} // namespace lsp