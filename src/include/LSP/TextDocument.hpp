#pragma once
#include "LSP/Uri.hpp"
#include "LSP/Protocol.hpp"
#include "Luau/Location.h"

size_t lspLength(const std::string& Code);

class TextDocument
{
private:
    lsp::DocumentUri _uri;
    std::string _languageId;
    size_t _version;
    std::string _content;
    mutable std::optional<std::vector<size_t>> _lineOffsets = std::nullopt;

public:
    TextDocument(const lsp::DocumentUri& uri, const std::string& languageId, size_t version, const std::string& content)
        : _uri(uri)
        , _languageId(languageId)
        , _version(version)
        , _content(content)
    {
    }

    const lsp::DocumentUri& uri() const
    {
        return _uri;
    }

    const std::string& languageId() const
    {
        return _languageId;
    }

    size_t version() const
    {
        return _version;
    }

    std::string getText(std::optional<lsp::Range> range = std::nullopt) const;

    lsp::Position positionAt(size_t offset) const;
    size_t offsetAt(const lsp::Position& position) const;

    Luau::Position convertPosition(const lsp::Position& position) const;

    bool applyChange(const lsp::TextDocumentContentChangeEvent& change);

    void update(const std::vector<lsp::TextDocumentContentChangeEvent>& changes, size_t version);

    const std::vector<size_t>& getLineOffsets() const;
    size_t lineCount() const;

    // TODO: this is a bit expensive
    std::vector<std::string_view> getLines() const;
};