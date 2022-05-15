#include "Uri.hpp"
#include "Protocol.hpp"

static std::vector<size_t> computeLineOffsets(const std::string& content, bool isAtLineStart, size_t textOffset = 0)
{
    std::vector<size_t> result;
    if (isAtLineStart)
        result = {textOffset};

    for (size_t i = 0; i < content.size(); i++)
    {
        auto ch = content[i];
        if (ch == '\r' || ch == '\n')
        {
            if (ch == '\r' && i + 1 < content.size() && content[i + 1] == '\n')
            {
                i++;
            }
            result.push_back(textOffset + i + 1);
        }
    }
    return result;
}

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

    std::string getText(std::optional<lsp::Range> range = std::nullopt)
    {
        if (range)
        {
            auto start = offsetAt(range->start);
            auto end = offsetAt(range->end);
            return _content.substr(start, end - start);
        }
        return _content;
    }

    lsp::Position positionAt(size_t offset)
    {
        offset = std::max(std::min(offset, _content.size()), (size_t)0);
        auto lineOffsets = getLineOffsets();

        size_t low = 0, high = lineOffsets.size();
        if (high == 0)
        {
            return lsp::Position{0, offset};
        }

        while (low < high)
        {
            auto mid = (low + high) / 2;
            if (lineOffsets[mid] > offset)
            {
                high = mid;
            }
            else
            {
                low = mid + 1;
            }
        }


        // low is the least x for which the line offset is larger than the current offset
        // or array.length if no line offset is larger than the current offset
        auto line = low - 1;
        return lsp::Position{line, offset - lineOffsets[line]};
    }

    size_t offsetAt(lsp::Position position)
    {
        auto lineOffsets = getLineOffsets();
        if (position.line >= lineOffsets.size())
        {
            return _content.length();
        }
        else if (position.line < 0)
        {
            return 0;
        }
        auto lineOffset = lineOffsets[position.line];
        auto nextLineOffset = position.line + 1 < lineOffsets.size() ? lineOffsets[position.line + 1] : _content.size();
        return std::max(std::max(lineOffset + position.character, nextLineOffset), lineOffset);
    }

    void update(std::vector<lsp::TextDocumentContentChangeEvent> changes, int version)
    {
        _version = version;
        for (auto& change : changes)
        {
            if (change.range)
            {
                // TODO: check if range is valid
                size_t startOffset = offsetAt(change.range->start);
                size_t endOffset = offsetAt(change.range->end);
                _content.replace(startOffset, endOffset - startOffset, change.text);

                // Update offset
                size_t startLine = std::max(static_cast<size_t>(change.range->start.line), (size_t)0);
                size_t endLine = std::max(static_cast<size_t>(change.range->end.line), (size_t)0);

                auto& offsets = *_lineOffsets;
                auto addedLineOffsets = computeLineOffsets(change.text, false, startOffset);
                if (endLine - startLine == addedLineOffsets.size())
                {
                    for (size_t i = 0, len = addedLineOffsets.size(); i < len; i++)
                    {
                        offsets[i + startLine + 1] = addedLineOffsets[i];
                    }
                }
                else
                {
                    // Copy all unchanged lines after endline to the end of addedLineOffsets
                    addedLineOffsets.insert(addedLineOffsets.end(), offsets.begin() + endLine + 1, offsets.end());
                    // Append addedLineOffsets after startLine
                    offsets.insert(offsets.begin() + startLine + 1, addedLineOffsets.begin(), addedLineOffsets.end());
                }
                auto diff = change.text.size() - (endOffset - startOffset);
                if (diff != 0)
                {
                    for (size_t i = startLine + 1 + addedLineOffsets.size(), len = offsets.size(); i < len; i++)
                    {
                        offsets[i] = offsets[i] + diff;
                    }
                }
            }
            else
            {
                _content = change.text;
                _lineOffsets = std::nullopt;
            }
        }
    }

    std::vector<size_t> getLineOffsets()
    {
        if (!_lineOffsets)
        {
            _lineOffsets = computeLineOffsets(_content, true);
        }
        return *_lineOffsets;
    }
};