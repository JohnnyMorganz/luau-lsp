#pragma once

namespace lsp
{
struct DidOpenTextDocumentParams
{
    TextDocumentItem textDocument;
};
NLOHMANN_DEFINE_OPTIONAL(DidOpenTextDocumentParams, textDocument);

struct TextDocumentContentChangeEvent
{
    // If only text is provided, then its considered to be the whole document
    std::optional<Range> range = std::nullopt;
    std::string text;
};
NLOHMANN_DEFINE_OPTIONAL(TextDocumentContentChangeEvent, range, text);

struct DidChangeTextDocumentParams
{
    VersionedTextDocumentIdentifier textDocument;
    std::vector<TextDocumentContentChangeEvent> contentChanges;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DidChangeTextDocumentParams, textDocument, contentChanges);

struct DidCloseTextDocumentParams
{
    TextDocumentIdentifier textDocument;
};
NLOHMANN_DEFINE_OPTIONAL(DidCloseTextDocumentParams, textDocument);
} // namespace lsp