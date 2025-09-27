#pragma once
#include "Protocol/Structures.hpp"
#include <string>

namespace lsp
{
enum CompilerRemarksOptimizationLevel
{
    None = 0,
    O1 = 1,
    O2 = 2,
};

struct BytecodeParams
{
    TextDocumentIdentifier textDocument;
    CompilerRemarksOptimizationLevel optimizationLevel = CompilerRemarksOptimizationLevel::O1;
};
NLOHMANN_DEFINE_OPTIONAL(BytecodeParams, textDocument, optimizationLevel)

using BytecodeResult = std::string;

struct CompilerRemarksParams
{
    TextDocumentIdentifier textDocument;
    CompilerRemarksOptimizationLevel optimizationLevel = CompilerRemarksOptimizationLevel::O1;
};
NLOHMANN_DEFINE_OPTIONAL(CompilerRemarksParams, textDocument, optimizationLevel)

using CompilerRemarksResult = std::string;

struct RequireGraphParams
{
    TextDocumentIdentifier textDocument;
    /// Whether the require graph should only include requires from the provided text document.
    /// If false, compute the require graph of all indexed modules
    bool fromTextDocumentOnly = false;
};
NLOHMANN_DEFINE_OPTIONAL(RequireGraphParams, textDocument, fromTextDocumentOnly)

/// Returns a dot-file representation
using RequireGraphResult = std::string;
} // namespace lsp
