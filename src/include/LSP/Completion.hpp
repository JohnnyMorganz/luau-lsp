#pragma once

/// Defining sort text levels assigned to completion items
/// Note that sort text is lexicographically
namespace SortText
{
using SortTextT = const char*;
static constexpr SortTextT PrioritisedSuggestion = "0";
static constexpr SortTextT TableProperties = "1";
static constexpr SortTextT CorrectTypeKind = "2";
static constexpr SortTextT CorrectFunctionResult = "3";
static constexpr SortTextT Default = "4";
static constexpr SortTextT WrongIndexType = "5";
static constexpr SortTextT MetatableIndex = "6";
static constexpr SortTextT AutoImportsRotriever = "65"; // Prioritized over regular auto-imports
static constexpr SortTextT AutoImports = "7";
static constexpr SortTextT AutoImportsAbsolute = "71";
static constexpr SortTextT Keywords = "8";
static constexpr SortTextT Deprioritized = "9";
} // namespace SortText
