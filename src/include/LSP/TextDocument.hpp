#pragma once
#include "LSP/Uri.hpp"
#include "Luau/Location.h"
#include "Protocol/Structures.hpp"
#include "Protocol/DocumentSync.hpp"

size_t lspLength(const std::string& Code);

class TextDocument
{
private:
    lsp::DocumentUri _uri;
    std::string _languageId;
    size_t _version;

protected:
    std::string _content;
    mutable std::optional<std::vector<size_t>> _lineOffsets = std::nullopt;

public:
    TextDocument(lsp::DocumentUri uri, std::string languageId, size_t version, std::string content)
        : _uri(std::move(uri))
        , _languageId(std::move(languageId))
        , _version(version)
        , _content(std::move(content))
    {
    }

    virtual ~TextDocument() = default;

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

    virtual std::string getText(std::optional<lsp::Range> range = std::nullopt) const;
    virtual std::string getLine(size_t index) const;

    virtual lsp::Position positionAt(size_t offset) const;
    virtual size_t offsetAt(const lsp::Position& position) const;

    virtual Luau::Position convertPosition(const lsp::Position& position) const;
    virtual lsp::Position convertPosition(const Luau::Position& position) const;

    void update(const std::vector<lsp::TextDocumentContentChangeEvent>& changes, size_t version);

    virtual const std::vector<size_t>& getLineOffsets() const;
    virtual size_t lineCount() const;
};
