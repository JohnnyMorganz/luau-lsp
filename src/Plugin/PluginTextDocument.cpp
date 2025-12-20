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
    return lineOffset + luauPos.column;
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

Luau::Position PluginTextDocument::convertPosition(const lsp::Position& position) const
{
    // First convert LSP position to Luau position in original source
    // This uses the base class conversion which handles UTF-16 to UTF-8
    Luau::Position originalPos = TextDocument::convertPosition(position);

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

    // Then convert original Luau position to LSP position
    // This uses the original content stored in base class
    return TextDocument::convertPosition(originalPos);
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
