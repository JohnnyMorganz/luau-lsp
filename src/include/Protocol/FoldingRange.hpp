#pragma once

#include <optional>

#include "Protocol/Structures.hpp"

namespace lsp
{

struct FoldingRangeParams
{
    /**
     * The text document.
     */
    TextDocumentIdentifier textDocument;
};
NLOHMANN_DEFINE_OPTIONAL(FoldingRangeParams, textDocument)

enum struct FoldingRangeKind
{
    /**
     * Folding range for a comment
     */
    Comment,
    /**
     * Folding range for imports or includes
     */
    Imports,
    /**
     * Folding range for a region (e.g. `#region`)
     */
    Region,
};
NLOHMANN_JSON_SERIALIZE_ENUM(FoldingRangeKind, {
                                                   {FoldingRangeKind::Comment, "comment"},
                                                   {FoldingRangeKind::Imports, "imports"},
                                                   {FoldingRangeKind::Region, "region"},
                                               })


/**
 * Represents a folding range. To be valid, start and end line must be bigger
 * than zero and smaller than the number of lines in the document. Clients
 * are free to ignore invalid ranges.
 */
struct FoldingRange
{
    /**
     * The zero-based start line of the range to fold. The folded area starts
     * after the line's last character. To be valid, the end must be zero or
     * larger and smaller than the number of lines in the document.
     */
    size_t startLine = 0;
    /**
     * The zero-based character offset from where the folded range starts. If
     * not defined, defaults to the length of the start line.
     */
    std::optional<size_t> startCharacter = std::nullopt;
    /**
     * The zero-based end line of the range to fold. The folded area ends with
     * the line's last character. To be valid, the end must be zero or larger
     * and smaller than the number of lines in the document.
     */
    size_t endLine = 0;
    /**
     * The zero-based character offset before the folded range ends. If not
     * defined, defaults to the length of the end line.
     */
    std::optional<size_t> endCharacter = std::nullopt;
    /**
     * Describes the kind of the folding range such as `comment` or `region`.
     * The kind is used to categorize folding ranges and used by commands like
     * 'Fold all comments'. See [FoldingRangeKind](#FoldingRangeKind) for an
     * enumeration of standardized kinds.
     */
    std::optional<FoldingRangeKind> kind;
    /**
     * The text that the client should show when the specified range is
     * collapsed. If not defined or not supported by the client, a default
     * will be chosen by the client.
     *
     * @since 3.17.0 - proposed
     */
    std::optional<std::string> collapsedText;
};
NLOHMANN_DEFINE_OPTIONAL(FoldingRange, startLine, startCharacter, endLine, endCharacter, kind, collapsedText)
} // namespace lsp