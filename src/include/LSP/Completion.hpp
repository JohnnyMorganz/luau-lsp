#pragma once

/// Defining sort text levels assigned to completion items
/// Note that sort text is lexicographically
namespace SortText
{
static constexpr const char* PrioritisedSuggestion = "0";
static constexpr const char* TableProperties = "1";
static constexpr const char* CorrectTypeKind = "2";
static constexpr const char* CorrectFunctionResult = "3";
static constexpr const char* Default = "4";
static constexpr const char* WrongIndexType = "5";
static constexpr const char* MetatableIndex = "6";
static constexpr const char* AutoImports = "7";
static constexpr const char* AutoImportsAbsolute = "71";
static constexpr const char* Keywords = "8";
static constexpr const char* Deprioritized = "9";
} // namespace SortText
