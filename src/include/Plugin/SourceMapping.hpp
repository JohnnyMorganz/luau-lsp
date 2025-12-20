#pragma once
#include "Plugin/TextEdit.hpp"
#include "Luau/Location.h"
#include <string>
#include <vector>
#include <optional>

namespace Luau::LanguageServer::Plugin
{

// A single edit that was applied, with both original and transformed ranges
struct AppliedEdit
{
    Luau::Location originalRange;     // Range in original that was replaced
    std::string newText;              // What it was replaced with
    Luau::Location transformedRange;  // Resulting range in transformed source
};

// Handles bidirectional position mapping between original and transformed source
class SourceMapping
{
    std::string originalSource;
    std::string transformedSource;
    std::vector<AppliedEdit> edits;  // Sorted by original position

public:
    SourceMapping() = default;
    SourceMapping(std::string original, std::string transformed, std::vector<AppliedEdit> appliedEdits);

    // Build mapping from a list of text edits applied to original source
    // Edits must not overlap. They will be sorted by position.
    static SourceMapping fromEdits(const std::string& originalSource, const std::vector<TextEdit>& edits);

    // Convert position from original source to transformed source
    // Returns nullopt if position falls inside a deleted region
    std::optional<Luau::Position> originalToTransformed(const Luau::Position& pos) const;

    // Convert position from transformed source to original source
    // Returns nullopt if position falls inside synthesized (inserted) text
    std::optional<Luau::Position> transformedToOriginal(const Luau::Position& pos) const;

    const std::string& getTransformedSource() const
    {
        return transformedSource;
    }

    const std::string& getOriginalSource() const
    {
        return originalSource;
    }

    const std::vector<AppliedEdit>& getEdits() const
    {
        return edits;
    }

    bool hasEdits() const
    {
        return !edits.empty();
    }
};

} // namespace Luau::LanguageServer::Plugin
