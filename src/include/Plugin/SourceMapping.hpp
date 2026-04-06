#pragma once
#include "Plugin/PluginTypes.hpp"
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

// Result of applying edits to source code
struct TransformResult
{
    std::string transformedSource;
    std::vector<AppliedEdit> edits;  // Sorted by original position
};

// Handles bidirectional position mapping between original and transformed source
class SourceMapping
{
    std::vector<AppliedEdit> edits;  // Sorted by original position

public:
    SourceMapping() = default;
    explicit SourceMapping(std::vector<AppliedEdit> appliedEdits);

    // Build transformed source and mapping from a list of text edits applied to original source
    // Edits must not overlap. They will be sorted by position.
    static TransformResult fromEdits(const std::string& originalSource, const std::vector<TextEdit>& edits);

    // Convert position from original source to transformed source
    // Returns nullopt if position falls inside a deleted region
    std::optional<Luau::Position> originalToTransformed(const Luau::Position& pos) const;

    // Convert position from transformed source to original source
    // Returns nullopt if position falls inside synthesized (inserted) text
    std::optional<Luau::Position> transformedToOriginal(const Luau::Position& pos) const;

    bool hasEdits() const
    {
        return !edits.empty();
    }
};

} // namespace Luau::LanguageServer::Plugin
