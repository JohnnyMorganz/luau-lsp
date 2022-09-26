#pragma once
#include "Luau/Ast.h"
#include "Luau/Module.h"
#include "LSP/Protocol.hpp"

struct SemanticToken
{
    Luau::Position start;
    Luau::Position end;
    lsp::SemanticTokenTypes tokenType;
    lsp::SemanticTokenModifiers tokenModifiers;
};

std::vector<SemanticToken> getSemanticTokens(Luau::ModulePtr module, Luau::SourceModule* sourceModule);