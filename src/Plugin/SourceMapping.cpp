#include "Plugin/SourceMapping.hpp"
#include <algorithm>
#include <stdexcept>

namespace Luau::LanguageServer::Plugin
{

SourceMapping::SourceMapping(std::string original, std::string transformed, std::vector<AppliedEdit> appliedEdits)
    : originalSource(std::move(original))
    , transformedSource(std::move(transformed))
    , edits(std::move(appliedEdits))
{
}

namespace
{

// Convert a Luau::Position to a byte offset in a string
size_t positionToOffset(const std::string& source, const Luau::Position& pos)
{
    size_t offset = 0;
    unsigned int currentLine = 0;

    while (offset < source.size() && currentLine < pos.line)
    {
        if (source[offset] == '\n')
            currentLine++;
        offset++;
    }

    // Add column offset (clamped to line length)
    size_t lineStart = offset;
    while (offset < source.size() && source[offset] != '\n' && (offset - lineStart) < pos.column)
        offset++;

    return offset;
}

// Calculate end position after inserting text at a given position
Luau::Position calculateEndPosition(const Luau::Position& start, const std::string& text)
{
    unsigned int line = start.line;
    unsigned int column = start.column;

    for (char c : text)
    {
        if (c == '\n')
        {
            line++;
            column = 0;
        }
        else
        {
            column++;
        }
    }

    return {line, column};
}

// Calculate the length of text in terms of lines and columns
struct TextSize
{
    int lineDelta;
    unsigned int lastLineLength;
};

TextSize calculateTextSize(const std::string& text)
{
    int lines = 0;
    unsigned int lastLineLength = 0;

    for (char c : text)
    {
        if (c == '\n')
        {
            lines++;
            lastLineLength = 0;
        }
        else
        {
            lastLineLength++;
        }
    }

    return {lines, lastLineLength};
}

} // anonymous namespace

SourceMapping SourceMapping::fromEdits(const std::string& originalSource, const std::vector<TextEdit>& edits)
{
    if (edits.empty())
        return SourceMapping{originalSource, originalSource, {}};

    // Sort edits by start position
    std::vector<TextEdit> sortedEdits = edits;
    std::sort(sortedEdits.begin(), sortedEdits.end(), [](const TextEdit& a, const TextEdit& b) {
        if (a.range.begin.line != b.range.begin.line)
            return a.range.begin.line < b.range.begin.line;
        return a.range.begin.column < b.range.begin.column;
    });

    // Validate no overlaps
    for (size_t i = 1; i < sortedEdits.size(); i++)
    {
        const auto& prev = sortedEdits[i - 1];
        const auto& curr = sortedEdits[i];

        bool overlaps = false;
        if (curr.range.begin.line < prev.range.end.line)
            overlaps = true;
        else if (curr.range.begin.line == prev.range.end.line && curr.range.begin.column < prev.range.end.column)
            overlaps = true;

        if (overlaps)
            throw std::runtime_error("Plugin returned overlapping edits");
    }

    // Apply edits and build mapping
    std::string transformed = originalSource;
    std::vector<AppliedEdit> appliedEdits;

    // Track cumulative offset in lines and columns
    // We need to adjust positions as we apply each edit
    int cumulativeLineDelta = 0;
    // Track cumulative column delta for edits on the same original line
    int cumulativeColumnDelta = 0;
    // Track which original line we're accumulating column deltas for (-1 means none)
    int currentOriginalLine = -1;

    for (const auto& edit : sortedEdits)
    {
        // Calculate the adjusted position in the current transformed source
        Luau::Position adjustedStart = edit.range.begin;
        Luau::Position adjustedEnd = edit.range.end;

        // Apply cumulative line delta
        adjustedStart.line += cumulativeLineDelta;
        adjustedEnd.line += cumulativeLineDelta;

        // If this edit is on the same original line as previous edits, apply cumulative column delta
        if (static_cast<int>(edit.range.begin.line) == currentOriginalLine)
        {
            adjustedStart.column += cumulativeColumnDelta;
            // If the edit is entirely on one line, also adjust the end column
            if (edit.range.begin.line == edit.range.end.line)
                adjustedEnd.column += cumulativeColumnDelta;
        }
        else
        {
            // New original line - reset column delta tracking
            cumulativeColumnDelta = 0;
            currentOriginalLine = static_cast<int>(edit.range.begin.line);
        }

        // Get byte offsets in current transformed source
        size_t startOffset = positionToOffset(transformed, adjustedStart);
        size_t endOffset = positionToOffset(transformed, adjustedEnd);

        // Calculate the size of the original text being replaced (in original coordinates)
        size_t originalRangeLength = 0;
        if (edit.range.begin.line == edit.range.end.line)
        {
            originalRangeLength = edit.range.end.column - edit.range.begin.column;
        }
        else
        {
            // For multiline edits, use the actual text length from transformed
            originalRangeLength = (endOffset > startOffset) ? (endOffset - startOffset) : 0;
        }

        // Calculate the size of the new text
        auto newSize = calculateTextSize(edit.newText);

        // Calculate transformed range
        Luau::Position transformedStart = adjustedStart;
        Luau::Position transformedEnd = calculateEndPosition(transformedStart, edit.newText);

        // Apply the edit to transformed source
        if (endOffset > transformed.size())
            endOffset = transformed.size();
        transformed = transformed.substr(0, startOffset) + edit.newText + transformed.substr(endOffset);

        // Store the applied edit
        appliedEdits.push_back(AppliedEdit{
            edit.range,                                      // originalRange
            edit.newText,                                    // newText
            Luau::Location{transformedStart, transformedEnd} // transformedRange
        });

        // Update cumulative line delta
        int originalLines = static_cast<int>(edit.range.end.line) - static_cast<int>(edit.range.begin.line);
        cumulativeLineDelta += (newSize.lineDelta - originalLines);

        // Update cumulative column delta for same-line tracking
        if (edit.range.begin.line == edit.range.end.line && newSize.lineDelta == 0)
        {
            // Single line edit staying on same line - accumulate column delta
            int editColumnDelta = static_cast<int>(edit.newText.size()) - static_cast<int>(originalRangeLength);
            cumulativeColumnDelta += editColumnDelta;
        }
        else
        {
            // Multiline edit - subsequent edits will be on different lines
            // Reset column tracking since line structure changed
            currentOriginalLine = -1;
            cumulativeColumnDelta = 0;
        }
    }

    return SourceMapping{originalSource, transformed, appliedEdits};
}

std::optional<Luau::Position> SourceMapping::originalToTransformed(const Luau::Position& pos) const
{
    if (edits.empty())
        return pos;

    // Track cumulative offset as we scan through edits
    int cumulativeLineDelta = 0;
    // Track cumulative column delta for edits on the same original line
    int cumulativeColumnDelta = 0;
    // Track which original line we're accumulating column deltas for (-1 means none)
    int currentOriginalLine = -1;

    for (const auto& edit : edits)
    {
        // Check if position is before this edit
        if (pos.line < edit.originalRange.begin.line ||
            (pos.line == edit.originalRange.begin.line && pos.column < edit.originalRange.begin.column))
        {
            // Position is before this edit - apply accumulated delta
            Luau::Position result = pos;
            result.line += cumulativeLineDelta;
            if (static_cast<int>(pos.line) == currentOriginalLine)
                result.column += cumulativeColumnDelta;
            return result;
        }

        // Check if position is inside this edit's original range
        bool insideEdit = false;
        if (pos.line > edit.originalRange.begin.line && pos.line < edit.originalRange.end.line)
            insideEdit = true;
        else if (pos.line == edit.originalRange.begin.line && pos.line == edit.originalRange.end.line)
            insideEdit = pos.column >= edit.originalRange.begin.column && pos.column < edit.originalRange.end.column;
        else if (pos.line == edit.originalRange.begin.line)
            insideEdit = pos.column >= edit.originalRange.begin.column;
        else if (pos.line == edit.originalRange.end.line)
            insideEdit = pos.column < edit.originalRange.end.column;

        if (insideEdit)
        {
            // Position is inside a deleted/replaced region
            // Map to start of transformed range
            return edit.transformedRange.begin;
        }

        // Position is after this edit - update cumulative deltas
        int originalLines = edit.originalRange.end.line - edit.originalRange.begin.line;
        int transformedLines = edit.transformedRange.end.line - edit.transformedRange.begin.line;
        cumulativeLineDelta += (transformedLines - originalLines);

        // Update cumulative column delta for same-line tracking
        if (edit.originalRange.end.line == edit.originalRange.begin.line &&
            edit.transformedRange.end.line == edit.transformedRange.begin.line)
        {
            // Single line edit - check if we're accumulating for this line
            if (static_cast<int>(edit.originalRange.begin.line) == currentOriginalLine)
            {
                // Same line as previous edit - accumulate
                int originalLen = edit.originalRange.end.column - edit.originalRange.begin.column;
                int transformedLen = edit.transformedRange.end.column - edit.transformedRange.begin.column;
                cumulativeColumnDelta += (transformedLen - originalLen);
            }
            else
            {
                // New line - start fresh
                currentOriginalLine = static_cast<int>(edit.originalRange.begin.line);
                int originalLen = edit.originalRange.end.column - edit.originalRange.begin.column;
                int transformedLen = edit.transformedRange.end.column - edit.transformedRange.begin.column;
                cumulativeColumnDelta = transformedLen - originalLen;
            }
        }
        else
        {
            // Multiline edit - reset column tracking
            currentOriginalLine = -1;
            cumulativeColumnDelta = 0;
        }
    }

    // Position is after all edits
    Luau::Position result = pos;
    result.line += cumulativeLineDelta;
    if (static_cast<int>(pos.line) == currentOriginalLine)
        result.column += cumulativeColumnDelta;
    return result;
}

std::optional<Luau::Position> SourceMapping::transformedToOriginal(const Luau::Position& pos) const
{
    if (edits.empty())
        return pos;

    // Track cumulative offset as we scan through edits
    int cumulativeLineDelta = 0;
    // Track cumulative column delta for edits on the same transformed line
    int cumulativeColumnDelta = 0;
    // Track which transformed line we're accumulating column deltas for (-1 means none)
    int currentTransformedLine = -1;

    for (const auto& edit : edits)
    {
        // Check if position is before this edit's transformed range
        if (pos.line < edit.transformedRange.begin.line ||
            (pos.line == edit.transformedRange.begin.line && pos.column < edit.transformedRange.begin.column))
        {
            // Position is before this edit - apply accumulated delta
            Luau::Position result = pos;
            result.line -= cumulativeLineDelta;
            if (static_cast<int>(pos.line) == currentTransformedLine)
                result.column -= cumulativeColumnDelta;
            return result;
        }

        // Check if position is inside this edit's transformed range (synthesized text)
        bool insideEdit = false;
        if (pos.line > edit.transformedRange.begin.line && pos.line < edit.transformedRange.end.line)
            insideEdit = true;
        else if (pos.line == edit.transformedRange.begin.line && pos.line == edit.transformedRange.end.line)
            insideEdit = pos.column >= edit.transformedRange.begin.column && pos.column < edit.transformedRange.end.column;
        else if (pos.line == edit.transformedRange.begin.line)
            insideEdit = pos.column >= edit.transformedRange.begin.column;
        else if (pos.line == edit.transformedRange.end.line)
            insideEdit = pos.column < edit.transformedRange.end.column;

        if (insideEdit)
        {
            // Position is inside synthesized text
            // Map to start of original range
            return edit.originalRange.begin;
        }

        // Position is after this edit - update cumulative deltas
        int originalLines = edit.originalRange.end.line - edit.originalRange.begin.line;
        int transformedLines = edit.transformedRange.end.line - edit.transformedRange.begin.line;
        cumulativeLineDelta += (transformedLines - originalLines);

        // Update cumulative column delta for same-line tracking
        if (edit.originalRange.end.line == edit.originalRange.begin.line &&
            edit.transformedRange.end.line == edit.transformedRange.begin.line)
        {
            // Single line edit - check if we're accumulating for this transformed line
            if (static_cast<int>(edit.transformedRange.begin.line) == currentTransformedLine)
            {
                // Same line as previous edit - accumulate
                int originalLen = edit.originalRange.end.column - edit.originalRange.begin.column;
                int transformedLen = edit.transformedRange.end.column - edit.transformedRange.begin.column;
                cumulativeColumnDelta += (transformedLen - originalLen);
            }
            else
            {
                // New line - start fresh
                currentTransformedLine = static_cast<int>(edit.transformedRange.begin.line);
                int originalLen = edit.originalRange.end.column - edit.originalRange.begin.column;
                int transformedLen = edit.transformedRange.end.column - edit.transformedRange.begin.column;
                cumulativeColumnDelta = transformedLen - originalLen;
            }
        }
        else
        {
            // Multiline edit - reset column tracking
            currentTransformedLine = -1;
            cumulativeColumnDelta = 0;
        }
    }

    // Position is after all edits
    Luau::Position result = pos;
    result.line -= cumulativeLineDelta;
    if (static_cast<int>(pos.line) == currentTransformedLine)
        result.column -= cumulativeColumnDelta;
    return result;
}

} // namespace Luau::LanguageServer::Plugin
