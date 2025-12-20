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

// Based off Luau::CodeGen::AssemblyOptions::Target
enum struct CodeGenTarget
{
    Host,
    A64,
    A64_NoFeatures,
    X64_Windows,
    X64_SystemV
};

NLOHMANN_JSON_SERIALIZE_ENUM(CodeGenTarget, {
                                                {CodeGenTarget::Host, "host"},
                                                {CodeGenTarget::A64, "a64"},
                                                {CodeGenTarget::A64_NoFeatures, "a64_nofeatures"},
                                                {CodeGenTarget::X64_Windows, "x64_windows"},
                                                {CodeGenTarget::X64_SystemV, "x64_systemv"},
                                            })


struct CodegenParams
{
    TextDocumentIdentifier textDocument;
    CompilerRemarksOptimizationLevel optimizationLevel = CompilerRemarksOptimizationLevel::O1;
    CodeGenTarget codeGenTarget = CodeGenTarget::Host;
};
NLOHMANN_DEFINE_OPTIONAL(CodegenParams, textDocument, optimizationLevel, codeGenTarget)

using CodegenResult = std::string;

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
