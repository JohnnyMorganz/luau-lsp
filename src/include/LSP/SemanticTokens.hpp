#pragma once
#include "Luau/Ast.h"
#include "Luau/Module.h"
#include "Protocol/SemanticTokens.hpp"

struct SemanticToken
{
    Luau::Position start;
    Luau::Position end;
    lsp::SemanticTokenTypes tokenType;
    lsp::SemanticTokenModifiers tokenModifiers;
};

std::vector<SemanticToken> getSemanticTokens(const Luau::ModulePtr& module, Luau::SourceModule* sourceModule);