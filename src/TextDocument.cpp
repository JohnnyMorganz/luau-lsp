#include <iostream>
#include <climits>
#include "Luau/Common.h"
#include "Luau/Location.h"
#include "Luau/StringUtils.h"
#include "LSP/TextDocument.hpp"
#include "LSP/LanguageServer.hpp"

static size_t countLeadingZeros(unsigned char n)
{
#ifdef _MSC_VER
    unsigned long rl;
    return _BitScanReverse(&rl, n) ? 31 - int(rl) - 24 : 8;
#else
    return n == 0 ? 8 : __builtin_clz(n) - 24;
#endif
}

static size_t countLeadingOnes(unsigned char n)
{
    return countLeadingZeros((unsigned char)~n);
}

// https://github.com/llvm/llvm-project/blob/main/clang-tools-extra/clangd/SourceCode.cpp
// Iterates over unicode codepoints in the (UTF-8) string. For each,
// invokes CB(UTF-8 length, UTF-16 length), and breaks if it returns true.
// Returns true if CB returned true, false if we hit the end of string.
//
// If the string is not valid UTF-8, we log this error and "decode" the
// text in some arbitrary way. This is pretty sad, but this tends to happen deep
// within indexing of headers where clang misdetected the encoding, and
// propagating the error all the way back up is (probably?) not be worth it.
using CodepointsCallback = std::function<bool(int, int)>;
static bool iterateCodepoints(const std::string& U8, const CodepointsCallback& CB)
{
    bool LoggedInvalid = false;
    // A codepoint takes two UTF-16 code unit if it's astral (outside BMP).
    // Astral codepoints are encoded as 4 bytes in UTF-8, starting with 11110xxx.
    for (size_t I = 0; I < U8.size();)
    {
        unsigned char C = static_cast<unsigned char>(U8[I]);
        if (LUAU_LIKELY(!(C & 0x80)))
        { // ASCII character.
            if (CB(1, 1))
                return true;
            ++I;
            continue;
        }
        // This convenient property of UTF-8 holds for all non-ASCII characters.
        size_t UTF8Length = countLeadingOnes(C);
        // 0xxx is ASCII, handled above. 10xxx is a trailing byte, invalid here.
        // 11111xxx is not valid UTF-8 at all, maybe some ISO-8859-*.
        if (LUAU_UNLIKELY(UTF8Length < 2 || UTF8Length > 4))
        {
            if (!LoggedInvalid)
            {
                std::cerr << "File has invalid UTF-8 near offset " << I << ": " << U8 << "\n";
                LoggedInvalid = true;
            }
            // We can't give a correct result, but avoid returning something wild.
            // Pretend this is a valid ASCII byte, for lack of better options.
            // (Too late to get ISO-8859-* right, we've skipped some bytes already).
            if (CB(1, 1))
                return true;
            ++I;
            continue;
        }
        I += UTF8Length; // Skip over all trailing bytes.
        // A codepoint takes two UTF-16 code unit if it's astral (outside BMP).
        // Astral codepoints are encoded as 4 bytes in UTF-8 (11110xxx ...)
        if (CB(static_cast<int>(UTF8Length), UTF8Length == 4 ? 2 : 1))
            return true;
    }
    return false;
}

// https://github.com/llvm/llvm-project/blob/main/clang-tools-extra/clangd/SourceCode.cpp
// Returns the byte offset into the string that is an offset of \p Units in
// the specified encoding.
// Conceptually, this converts to the encoding, truncates to CodeUnits,
// converts back to UTF-8, and returns the length in bytes.
static size_t measureUnits(const std::string& U8, int Units, lsp::PositionEncodingKind Enc, bool& Valid)
{
    Valid = Units >= 0;
    if (Units <= 0)
        return 0;
    size_t Result = 0;
    switch (Enc)
    {
    case lsp::PositionEncodingKind::UTF8:
        Result = Units;
        break;
    case lsp::PositionEncodingKind::UTF16:
        Valid = iterateCodepoints(U8,
            [&](int U8Len, int U16Len)
            {
                Result += U8Len;
                Units -= U16Len;
                return Units <= 0;
            });
        if (Units < 0) // Offset in the middle of a surrogate pair.
            Valid = false;
        break;
    case lsp::PositionEncodingKind::UTF32:
        Valid = iterateCodepoints(U8,
            [&](int U8Len, int U16Len)
            {
                Result += U8Len;
                Units--;
                return Units <= 0;
            });
        break;
        // case OffsetEncoding::UnsupportedEncoding:
        //     llvm_unreachable("unsupported encoding");
    }
    // Don't return an out-of-range index if we overran.
    if (Result > U8.size())
    {
        Valid = false;
        return U8.size();
    }
    return Result;
}

// https://github.com/llvm/llvm-project/blob/main/clang-tools-extra/clangd/SourceCode.cpp
size_t lspLength(const std::string& Code)
{
    size_t Count = 0;
    switch (positionEncoding())
    {
    case lsp::PositionEncodingKind::UTF8:
        Count = Code.size();
        break;
    case lsp::PositionEncodingKind::UTF16:
        iterateCodepoints(Code,
            [&](int U8Len, int U16Len)
            {
                Count += U16Len;
                return false;
            });
        break;
    case lsp::PositionEncodingKind::UTF32:
        iterateCodepoints(Code,
            [&](int U8Len, int U16Len)
            {
                ++Count;
                return false;
            });
        break;
        // case OffsetEncoding::UnsupportedEncoding:
        //     llvm_unreachable("unsupported encoding");
    }
    return Count;
}

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


std::string TextDocument::getText(std::optional<lsp::Range> range) const
{
    if (range)
    {
        auto start = offsetAt(range->start);
        auto end = offsetAt(range->end);
        return _content.substr(start, end - start);
    }
    return _content;
}

std::string TextDocument::getLine(size_t index) const
{
    LUAU_ASSERT(index < lineCount());
    auto lineOffsets = getLineOffsets();
    auto startOffset = lineOffsets[index];
    auto nextLineOffset = index + 1 < lineCount() ? lineOffsets[index + 1] : _content.size();
    return _content.substr(startOffset, nextLineOffset - startOffset - 1);
}

lsp::Position TextDocument::positionAt(size_t offset) const
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
    std::string currentContent = _content.substr(lineOffsets[line], offset - lineOffsets[line]);
    return lsp::Position{line, lspLength(currentContent)};
}

size_t TextDocument::offsetAt(const lsp::Position& position) const
{
    auto utf8Position = convertPosition(position);
    auto lineOffsets = getLineOffsets();
    auto lineOffset = lineOffsets[utf8Position.line];
    return lineOffset + utf8Position.column;
}

// We treat all lsp:Positions as UTF-16 encoded. We must convert between the two when necessary
Luau::Position TextDocument::convertPosition(const lsp::Position& position) const
{
    LUAU_ASSERT(position.line <= UINT_MAX);
    LUAU_ASSERT(position.character <= UINT_MAX);

    auto lineOffsets = getLineOffsets();
    if (position.line >= lineCount())
    {
        return Luau::Position{static_cast<unsigned int>(lineOffsets.size() - 1), static_cast<unsigned int>(_content.size() - lineOffsets.back())};
    }
    else if (position.line < 0)
    {
        return Luau::Position{0, 0};
    }
    auto lineOffset = lineOffsets[position.line];
    auto nextLineOffset = position.line + 1 < lineOffsets.size() ? lineOffsets[position.line + 1] : _content.size();

    // position.character may be in UTF-16, so we need to convert as necessary
    bool valid;
    std::string line = _content.substr(lineOffset, nextLineOffset - lineOffset);
    size_t byteInLine = measureUnits(line, static_cast<int>(position.character), positionEncoding(), valid);

    if (!valid)
        std::cerr << "UTF-16 offset " << position.character << " is invalid for line " << position.line << "\n";

    return Luau::Position{static_cast<unsigned int>(position.line), static_cast<unsigned int>(byteInLine)};
}

lsp::Position TextDocument::convertPosition(const Luau::Position& position) const
{
    auto lineOffsets = getLineOffsets();
    auto line = position.line;
    std::string currentContent = _content.substr(lineOffsets[line], position.column);
    return lsp::Position{line, lspLength(currentContent)};
}

void TextDocument::update(const std::vector<lsp::TextDocumentContentChangeEvent>& changes, size_t version)
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

size_t TextDocument::lineCount() const
{
    return getLineOffsets().size();
}

const std::vector<size_t>& TextDocument::getLineOffsets() const
{
    if (!_lineOffsets)
    {
        _lineOffsets = computeLineOffsets(_content, true);
    }
    return *_lineOffsets;
}