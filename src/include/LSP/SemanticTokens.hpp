#include "Luau/Ast.h"
#include "LSP/Protocol.hpp"

struct SemanticToken
{
    Luau::Position start;
    Luau::Position end;
    lsp::SemanticTokenTypes tokenType;
    lsp::SemanticTokenModifiers tokenModifiers;
};

std::vector<SemanticToken> getSemanticTokens(Luau::AstStatBlock* block);