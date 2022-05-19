#pragma once
#include "LSP/Uri.hpp"
#include "LSP/Protocol.hpp"

class TextDocument
{
private:
    lsp::DocumentUri _uri;
    std::string _languageId;
    int _version;
    std::string _content;
    std::optional<std::vector<size_t>> _lineOffsets = std::nullopt;

public:
    TextDocument(const lsp::DocumentUri& uri, const std::string& languageId, int version, const std::string& content)
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

    int version() const
    {
        return _version;
    }

    std::string getText(std::optional<lsp::Range> range = std::nullopt);

    // lsp::Position positionAt(size_t offset);
    // TODO: fix issues
    // size_t offsetAt(lsp::Position position);

    size_t offsetAt(const lsp::Position& position);

    bool applyChange(const lsp::TextDocumentContentChangeEvent& change);

    void update(std::vector<lsp::TextDocumentContentChangeEvent> changes, int version);

    std::vector<size_t> getLineOffsets();

    // TODO: this is a bit expensive
    std::vector<std::string_view> getLines();
};