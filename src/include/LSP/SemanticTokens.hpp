#pragma once
#include "Luau/Location.h"
#include "Luau/Module.h"
#include "Luau/Frontend.h"
#include "Protocol/SemanticTokens.hpp"

struct SemanticToken
{
    Luau::Position start;
    Luau::Position end;
    lsp::SemanticTokenTypes tokenType;
    lsp::SemanticTokenModifiers tokenModifiers;
};

std::vector<SemanticToken> getSemanticTokens(const Luau::Frontend& frontend, const Luau::ModulePtr& module, const Luau::SourceModule* sourceModule);
