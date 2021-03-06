#pragma once
#include "LSP/Uri.hpp"
#include "LSP/Protocol.hpp"

class TextDocument
{
private:
    lsp::DocumentUri _uri;
    std::string _languageId;
    size_t _version;
    std::string _content;
    std::optional<std::vector<size_t>> _lineOffsets = std::nullopt;

public:
    TextDocument(const lsp::DocumentUri& uri, const std::string& languageId, size_t version, const std::string& content)
        : _uri(uri)
        , _languageId(languageId)
        , _version(version)
        , _content(content)
    {
    }

    lsp::DocumentUri uri() const
    {
        return _uri;
    }

    std::string languageId() const
    {
        return _languageId;
    }

    size_t version() const
    {
        return _version;
    }

    std::string getText(std::optional<lsp::Range> range = std::nullopt);

    lsp::Position positionAt(size_t offset);
    size_t offsetAt(lsp::Position position);

    bool applyChange(const lsp::TextDocumentContentChangeEvent& change);

    void update(std::vector<lsp::TextDocumentContentChangeEvent> changes, size_t version);

    std::vector<size_t> getLineOffsets();
    size_t lineCount();

    // TODO: this is a bit expensive
    std::vector<std::string_view> getLines();
};