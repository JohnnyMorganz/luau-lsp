#include <iostream>
#include "Luau/StringUtils.h"
#include "LSP/TextDocument.hpp"

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

static lsp::Range getWellformedRange(lsp::Range range)
{
    auto start = range.start;
    auto end = range.end;
    if (start.line > end.line || (start.line == end.line && start.character > end.character))
    {
        return lsp::Range{end, start};
    }
    return range;
}


std::string TextDocument::getText(std::optional<lsp::Range> range)
{
    if (range)
    {
        auto start = offsetAt(range->start);
        auto end = offsetAt(range->end);
        return _content.substr(start, end - start);
    }
    return _content;
}

lsp::Position TextDocument::positionAt(size_t offset)
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

size_t TextDocument::offsetAt(lsp::Position position)
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
    return std::max(std::min(lineOffset + position.character, nextLineOffset), lineOffset);
}

void TextDocument::update(std::vector<lsp::TextDocumentContentChangeEvent> changes, size_t version)
{
    _version = version;
    for (auto& change : changes)
    {
        if (change.range)
        {
            auto range = getWellformedRange(*change.range);
            size_t startOffset = offsetAt(range.start);
            size_t endOffset = offsetAt(range.end); // End position is EXCLUSIVE
            _content = _content.substr(0, startOffset) + change.text + _content.substr(endOffset, _content.size() - endOffset);

            // Update offset
            size_t startLine = std::max(range.start.line, (size_t)0);
            size_t endLine = std::max(range.end.line, (size_t)0);

            auto& offsets = *_lineOffsets;
            auto addedLineOffsets = computeLineOffsets(change.text, false, startOffset);
            auto addedLineOffsetsLen = addedLineOffsets.size();
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
                if (offsets.size() >= endLine)
                    addedLineOffsets.insert(addedLineOffsets.end(), offsets.begin() + endLine + 1, offsets.end());
                // Erase all points from offsets after startline
                if (offsets.size() >= startLine)
                    offsets.erase(offsets.begin() + startLine + 1, offsets.end());
                // Append addedLineOffsets to offsets
                offsets.insert(offsets.end(), addedLineOffsets.begin(), addedLineOffsets.end());
            }
            long diff = static_cast<long>(change.text.size() - (endOffset - startOffset));
            if (diff != 0)
            {
                for (size_t i = startLine + 1 + addedLineOffsetsLen, len = offsets.size(); i < len; i++)
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

size_t TextDocument::lineCount()
{
    return getLineOffsets().size();
}

std::vector<size_t> TextDocument::getLineOffsets()
{
    if (!_lineOffsets)
    {
        _lineOffsets = computeLineOffsets(_content, true);
    }
    return *_lineOffsets;
}

// TODO: this is a bit expensive
std::vector<std::string_view> TextDocument::getLines()
{
    return Luau::split(_content, '\n');
}