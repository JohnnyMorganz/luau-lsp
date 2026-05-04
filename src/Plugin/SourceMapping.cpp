#include "Plugin/SourceMapping.hpp"
#include <algorithm>
#include <stdexcept>

namespace Luau::LanguageServer::Plugin
{

SourceMapping::SourceMapping(std::vector<AppliedEdit> appliedEdits)
    : edits(std::move(appliedEdits))
{
}

namespace
{

// Calculate the size of text in terms of line delta and last line length
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

TransformResult SourceMapping::fromEdits(const std::string& originalSource, const std::vector<TextEdit>& edits)
{
    if (edits.empty())
        return TransformResult{std::string(originalSource), std::vector<AppliedEdit>{}};

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

    // Build transformed string in a single pass over originalSource, computing
    // position mappings via cumulative line/column deltas.
    // We carry forward the byte offset to avoid re-scanning from the start for each edit.
    std::string transformed;
    transformed.reserve(originalSource.size());
    std::vector<AppliedEdit> appliedEdits;
    size_t lastOffset = 0;

    int cumulativeLineDelta = 0;
    int cumulativeColumnDelta = 0;
    int currentOriginalLine = -1;

    // State for carry-forward positionToOffset
    size_t scanOffset = 0;
    unsigned int scanLine = 0;

    // Convert a Luau::Position to a byte offset, carrying forward from previous scan position.
    // Requires that positions are queried in non-decreasing order.
    auto positionToOffset = [&](const Luau::Position& pos) -> size_t {
        while (scanOffset < originalSource.size() && scanLine < pos.line)
        {
            if (originalSource[scanOffset] == '\n')
                scanLine++;
            scanOffset++;
        }

        // Add column offset (clamped to line length)
        size_t lineStart = scanOffset;
        size_t result = scanOffset;
        while (result < originalSource.size() && originalSource[result] != '\n' && (result - lineStart) < pos.column)
            result++;

        return result;
    };

    for (const auto& edit : sortedEdits)
    {
        // Convert original positions to byte offsets (carried forward)
        size_t startOffset = positionToOffset(edit.range.begin);
        size_t endOffset = positionToOffset(edit.range.end);
        if (endOffset > originalSource.size())
            endOffset = originalSource.size();

        // Copy unchanged text before this edit, then insert replacement
        transformed.append(originalSource, lastOffset, startOffset - lastOffset);
        transformed.append(edit.newText);
        lastOffset = endOffset;

        // Compute transformed start position via cumulative deltas
        Luau::Position transformedStart = edit.range.begin;
        transformedStart.line += cumulativeLineDelta;
        if (static_cast<int>(edit.range.begin.line) == currentOriginalLine)
            transformedStart.column += cumulativeColumnDelta;

        // Compute transformed end position from start + text size
        auto newSize = calculateTextSize(edit.newText);
        Luau::Position transformedEnd = newSize.lineDelta == 0
            ? Luau::Position{transformedStart.line, transformedStart.column + newSize.lastLineLength}
            : Luau::Position{transformedStart.line + static_cast<unsigned int>(newSize.lineDelta), newSize.lastLineLength};

        appliedEdits.push_back(AppliedEdit{
            edit.range,
            edit.newText,
            Luau::Location{transformedStart, transformedEnd},
        });

        // Update cumulative deltas
        int originalLines = static_cast<int>(edit.range.end.line) - static_cast<int>(edit.range.begin.line);
        cumulativeLineDelta += (newSize.lineDelta - originalLines);

        size_t originalRangeLength = endOffset - startOffset;
        if (edit.range.begin.line == edit.range.end.line && newSize.lineDelta == 0)
        {
            if (static_cast<int>(edit.range.begin.line) != currentOriginalLine)
            {
                cumulativeColumnDelta = 0;
                currentOriginalLine = static_cast<int>(edit.range.begin.line);
            }
            cumulativeColumnDelta += static_cast<int>(edit.newText.size()) - static_cast<int>(originalRangeLength);
        }
        else
        {
            currentOriginalLine = -1;
            cumulativeColumnDelta = 0;
        }
    }

    // Copy remaining text after last edit
    transformed.append(originalSource, lastOffset, originalSource.size() - lastOffset);

    return TransformResult{std::move(transformed), std::move(appliedEdits)};
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
