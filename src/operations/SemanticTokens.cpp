#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/Protocol.hpp"
#include "LSP/LuauExt.hpp"

struct SemanticToken
{
    Luau::Position start;
    Luau::Position end;
    lsp::SemanticTokenTypes tokenType;
    std::vector<lsp::SemanticTokenModifiers> tokenModifiers;
};

struct SemanticTokensVisitor : public Luau::AstVisitor
{
    std::vector<SemanticToken> tokens;

    // bool visit(Luau::AstStatLocal* local) override
    // {
    //     for (size_t i = 0; i < local->vars.size; ++i)
    //     {
    //         createLocalSymbol(local->vars.data[i]);
    //         // TODO: if the value assigned is a table, should we include its properties?
    //     }
    //     return false;
    // }

    // bool visit(Luau::AstStatFunction* function) override
    // {
    //     lsp::DocumentSymbol symbol;
    //     symbol.name = Luau::toString(function->name);
    //     symbol.kind = lsp::SymbolKind::Function;
    //     symbol.range = {convertPosition(function->location.begin), convertPosition(function->location.end)};
    //     symbol.selectionRange = {convertPosition(function->name->location.begin), convertPosition(function->name->location.end)};
    //     visitFunction(function->func, symbol);
    //     addSymbol(symbol);
    //     return false;
    // }

    // bool visit(Luau::AstStatLocalFunction* func) override
    // {
    //     lsp::DocumentSymbol symbol;
    //     symbol.name = Luau::toString(func->name);
    //     symbol.kind = lsp::SymbolKind::Function;
    //     symbol.range = {convertPosition(func->location.begin), convertPosition(func->location.end)};
    //     symbol.selectionRange = {convertPosition(func->name->location.begin), convertPosition(func->name->location.end)};
    //     visitFunction(func->func, symbol);
    //     addSymbol(symbol);
    //     return false;
    // }

    // bool visit(Luau::AstStatTypeAlias* alias) override
    // {
    //     lsp::DocumentSymbol symbol;
    //     symbol.name = alias->name.value;
    //     symbol.kind = lsp::SymbolKind::Interface;
    //     symbol.range = {convertPosition(alias->location.begin), convertPosition(alias->location.end)};
    //     symbol.selectionRange = symbol.range;
    //     addSymbol(symbol);
    //     return false;
    // }

    // void visitFunction(Luau::AstExprFunction* func, lsp::DocumentSymbol& symbol)
    // {
    //     auto oldParent = parent;
    //     parent = &symbol;

    //     // Create parameter symbols + build function detail
    //     std::string detail = "function (";
    //     bool comma = false;
    //     for (auto* arg : func->args)
    //     {
    //         createLocalSymbol(arg);
    //         if (comma)
    //             detail += ", ";
    //         detail += arg->name.value;
    //         comma = true;
    //     }
    //     if (func->vararg)
    //     {
    //         lsp::DocumentSymbol symbol;
    //         symbol.name = "...";
    //         symbol.kind = lsp::SymbolKind::Variable;
    //         symbol.range = {convertPosition(func->varargLocation.begin), convertPosition(func->varargLocation.end)};
    //         addSymbol(symbol);

    //         if (comma)
    //             detail += ", ";
    //         detail += "...";
    //     }
    //     detail += ")";
    //     symbol.detail = detail;

    //     // Create symbols for body
    //     for (Luau::AstStat* stat : func->body->body)
    //     {
    //         stat->visit(this);
    //     }

    //     parent = oldParent;
    // }

    // bool visit(Luau::AstStatBlock* block) override
    // {
    //     for (Luau::AstStat* stat : block->body)
    //     {
    //         stat->visit(this);
    //     }

    //     return false;
    // }
};

size_t convertTokenType(const lsp::SemanticTokenTypes tokenType)
{
    return static_cast<size_t>(tokenType);
}

size_t convertTokenModifiers(const std::vector<lsp::SemanticTokenModifiers>& tokenModifiers)
{
    // Bit pack the token modifiers
}

std::vector<size_t> packTokens(const std::vector<SemanticToken>& tokens)
{
    // Sort the tokens into the correct ordering
    std::sort(tokens.begin(), tokens.end(),
        [](const SemanticToken& a, const SemanticToken& b)
        {
            return a.start < b.start;
        });

    // Pack the tokens
    std::vector<size_t> result;
    result.reserve(tokens.size() * 5); // Each token will take up 5 slots in the result

    size_t lastLine = 0;
    size_t lastChar = 0;

    for (auto& token : tokens)
    {
        auto line = token.start.line;
        auto startChar = token.start.column;

        auto deltaLine = line - lastLine;
        auto deltaStartChar = deltaLine == 0 ? startChar - lastChar : startChar;
        auto length = token.end.column - token.start.column + 1;

        result.insert(
            result.end(), {deltaLine, deltaStartChar, length, convertTokenType(token.tokenType), convertTokenModifiers(token.tokenModifiers)});

        lastLine = line;
        lastChar = startChar;
    }

    return result;
}

std::optional<std::vector<size_t>> WorkspaceFolder::semanticTokens(const lsp::SemanticTokensParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);

    // Run the type checker to ensure we are up to date
    if (frontend.isDirty(moduleName))
        frontend.check(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return std::nullopt;

    SemanticTokensVisitor visitor;
    visitor.visit(sourceModule->root);
    return packTokens(visitor.tokens);
}

std::optional<std::vector<size_t>> LanguageServer::semanticTokens(const lsp::SemanticTokensParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->semanticTokens(params);
}