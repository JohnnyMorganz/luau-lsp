#include "Plugin/PluginTextDocument.hpp"
#include "Luau/Common.h"

namespace Luau::LanguageServer::Plugin
{

// Helper to compute line offsets (same as in TextDocument.cpp)
static std::vector<size_t> computeLineOffsets(const std::string& content, bool isAtLineStart, size_t textOffset = 0)
{
    std::vector<size_t> result{};
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

PluginTextDocument::PluginTextDocument(
    lsp::DocumentUri uri,
    std::string languageId,
    size_t version,
    std::string originalContent,
    std::string transformedContent,
    SourceMapping mapping)
    : TextDocument(std::move(uri), std::move(languageId), version, std::move(originalContent))
    , mapping(std::move(mapping))
    , transformedContent(std::move(transformedContent))
{
}

std::string PluginTextDocument::getText(std::optional<lsp::Range> range) const
{
    if (range)
    {
        // Get the range from transformed content
        auto start = offsetAt(range->start);
        auto end = offsetAt(range->end);
        
        // Bounds checking
        start = std::min(start, transformedContent.size());
        end = std::min(end, transformedContent.size());
        if (start > end)
            std::swap(start, end);
            
        return transformedContent.substr(start, end - start);
    }

    // Handle shebang in transformed content
    if (transformedContent.size() > 2 && transformedContent[0] == '#' && transformedContent[1] == '!')
    {
        if (auto pos = transformedContent.find('\n'); pos != std::string::npos)
            return transformedContent.substr(pos);
        else
            return "\n";
    }
    return transformedContent;
}

std::string PluginTextDocument::getLine(size_t index) const
{
    LUAU_ASSERT(index < lineCount());
    auto lineOffsets = getLineOffsets();
    auto startOffset = lineOffsets[index];

    if (index + 1 < lineCount())
        return transformedContent.substr(startOffset, lineOffsets[index + 1] - startOffset - 1);
    else
        return transformedContent.substr(startOffset, std::string::npos);
}

size_t PluginTextDocument::offsetAt(const lsp::Position& position) const
{
    // Convert LSP position to Luau position in transformed content
    auto luauPos = convertPosition(position);
    auto lineOffsets = getLineOffsets();

    if (luauPos.line >= lineOffsets.size())
        return transformedContent.size();

    auto lineOffset = lineOffsets[luauPos.line];
    size_t offset = lineOffset + luauPos.column;
    
    // Clamp to content size to prevent out-of-bounds access
    return std::min(offset, transformedContent.size());
}

lsp::Position PluginTextDocument::positionAt(size_t offset) const
{
    // Convert offset in transformed content to position
    offset = std::max(std::min(offset, transformedContent.size()), (size_t)0);
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

    auto line = low - 1;

    // Get Luau position in transformed
    Luau::Position transformedPos{static_cast<unsigned int>(line), static_cast<unsigned int>(offset - lineOffsets[line])};

    // Map back to original position and return as LSP position
    return convertPosition(transformedPos);
}

const std::vector<size_t>& PluginTextDocument::getOriginalLineOffsets() const
{
    if (!_originalLineOffsets)
    {
        _originalLineOffsets = computeLineOffsets(_content, true);
    }
    return *_originalLineOffsets;
}

Luau::Position PluginTextDocument::convertPosition(const lsp::Position& position) const
{
    // Convert LSP position to Luau position in ORIGINAL source
    // We must use original line offsets (not transformed) because getLineOffsets() is virtual
    // and returns transformed offsets, which would break TextDocument::convertPosition
    
    auto originalLineOffsets = getOriginalLineOffsets();
    
    // Bounds check on line
    if (position.line >= originalLineOffsets.size())
    {
        // Clamp to end of original content
        size_t lastLine = originalLineOffsets.size() - 1;
        size_t lastLineStart = originalLineOffsets[lastLine];
        size_t lastLineLen = _content.size() - lastLineStart;
        Luau::Position originalPos{static_cast<unsigned int>(lastLine), static_cast<unsigned int>(lastLineLen)};
        
        if (auto transformedPos = mapping.originalToTransformed(originalPos))
            return *transformedPos;
        return originalPos;
    }
    
    // Get line content for UTF-16 to UTF-8 conversion
    size_t lineStart = originalLineOffsets[position.line];
    size_t nextLineStart = (position.line + 1 < originalLineOffsets.size()) 
        ? originalLineOffsets[position.line + 1] 
        : _content.size();
    std::string lineContent = _content.substr(lineStart, nextLineStart - lineStart);
    
    // Convert UTF-16 character offset to UTF-8 byte offset
    size_t utf8Column = 0;
    size_t utf16Units = 0;
    for (size_t i = 0; i < lineContent.size() && utf16Units < position.character; )
    {
        unsigned char c = lineContent[i];
        size_t charBytes = 1;
        size_t charUtf16Units = 1;
        
        if ((c & 0x80) == 0) {
            charBytes = 1;
            charUtf16Units = 1;
        } else if ((c & 0xE0) == 0xC0) {
            charBytes = 2;
            charUtf16Units = 1;
        } else if ((c & 0xF0) == 0xE0) {
            charBytes = 3;
            charUtf16Units = 1;
        } else if ((c & 0xF8) == 0xF0) {
            charBytes = 4;
            charUtf16Units = 2;  // Surrogate pair
        }
        
        if (i + charBytes > lineContent.size())
            break;
            
        utf8Column = i + charBytes;
        utf16Units += charUtf16Units;
        i += charBytes;
    }
    
    Luau::Position originalPos{static_cast<unsigned int>(position.line), static_cast<unsigned int>(utf8Column)};

    // Then map from original to transformed position
    if (auto transformedPos = mapping.originalToTransformed(originalPos))
        return *transformedPos;

    // Fallback if mapping fails
    return originalPos;
}

lsp::Position PluginTextDocument::convertPosition(const Luau::Position& position) const
{
    // First map from transformed to original position
    Luau::Position originalPos = position;
    if (auto mapped = mapping.transformedToOriginal(position))
        originalPos = *mapped;

    // Convert original Luau position to LSP position using ORIGINAL line offsets
    auto originalLineOffsets = getOriginalLineOffsets();

    // Bounds check
    if (originalPos.line >= originalLineOffsets.size())
    {
        // Clamp to last valid position
        size_t lastLine = originalLineOffsets.size() - 1;
        size_t lastLineStart = originalLineOffsets[lastLine];
        size_t lastLineLen = _content.size() - lastLineStart;
        return lsp::Position{lastLine, lastLineLen};
    }

    // Get the line content and compute UTF-16 length
    size_t lineStart = originalLineOffsets[originalPos.line];
    size_t columnBytes = std::min(static_cast<size_t>(originalPos.column), _content.size() - lineStart);
    std::string prefix = _content.substr(lineStart, columnBytes);
    
    // Use lspLength to convert UTF-8 bytes to UTF-16 code units
    return lsp::Position{originalPos.line, lspLength(prefix)};
}

const std::vector<size_t>& PluginTextDocument::getLineOffsets() const
{
    if (!_transformedLineOffsets)
    {
        _transformedLineOffsets = computeLineOffsets(transformedContent, true);
    }
    return *_transformedLineOffsets;
}

size_t PluginTextDocument::lineCount() const
{
    return getLineOffsets().size();
}

std::string PluginTextDocument::getOriginalText() const
{
    return _content;
}

} // namespace Luau::LanguageServer::Plugin
