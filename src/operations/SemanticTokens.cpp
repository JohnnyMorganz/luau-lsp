#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/Protocol.hpp"
#include "LSP/LuauExt.hpp"
#include "LSP/SemanticTokens.hpp"

struct SemanticTokensVisitor : public Luau::AstVisitor
{
    Luau::ModulePtr module;
    std::vector<SemanticToken> tokens;
    std::unordered_map<Luau::AstLocal*, lsp::SemanticTokenTypes> localTypes;

    explicit SemanticTokensVisitor(Luau::ModulePtr module)
        : module(module)
    {
    }

    bool visit(Luau::AstType* type) override
    {
        return true;
    }

    bool visit(Luau::AstTypeReference* ref) override
    {
        tokens.emplace_back(SemanticToken{ref->location.begin, ref->location.end, lsp::SemanticTokenTypes::Type, lsp::SemanticTokenModifiers::None});
        return false;
    }

    // bool visit(Luau::AstStatLocal* local) override
    // {
    //     for (size_t i = 0; i < local->vars.size; ++i)
    //     {
    //         createLocalSymbol(local->vars.data[i]);
    //         // TODO: if the value assigned is a table, should we include its properties?
    //     }
    //     return false;
    // }

    bool visit(Luau::AstStatFunction* func) override
    {
        tokens.emplace_back(SemanticToken{
            func->name->location.begin, func->name->location.end, lsp::SemanticTokenTypes::Function, lsp::SemanticTokenModifiers::None});
        return true;
    }

    bool visit(Luau::AstStatLocalFunction* func) override
    {
        tokens.emplace_back(SemanticToken{
            func->name->location.begin, func->name->location.end, lsp::SemanticTokenTypes::Function, lsp::SemanticTokenModifiers::None});
        return true;
    }

    bool visit(Luau::AstExprFunction* func) override
    {
        for (auto arg : func->args)
        {
            tokens.emplace_back(
                SemanticToken{arg->location.begin, arg->location.end, lsp::SemanticTokenTypes::Parameter, lsp::SemanticTokenModifiers::Declaration});
            localTypes.emplace(arg, lsp::SemanticTokenTypes::Parameter);
        }
        return true;
    }

    bool visit(Luau::AstExprLocal* local) override
    {
        auto type = lsp::SemanticTokenTypes::Variable;
        if (contains(localTypes, local->local))
            type = localTypes.at(local->local);
        tokens.emplace_back(SemanticToken{local->location.begin, local->location.end, type, lsp::SemanticTokenModifiers::None});
        return false;
    }

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

    bool visit(Luau::AstStatBlock* block) override
    {
        for (Luau::AstStat* stat : block->body)
        {
            stat->visit(this);
        }

        return false;
    }
};

std::vector<SemanticToken> getSemanticTokens(Luau::ModulePtr module, Luau::SourceModule* sourceModule)
{
    SemanticTokensVisitor visitor{module};
    visitor.visit(sourceModule->root);
    return visitor.tokens;
}

size_t convertTokenType(const lsp::SemanticTokenTypes tokenType)
{
    return static_cast<size_t>(tokenType);
}

std::vector<size_t> packTokens(std::vector<SemanticToken>& tokens)
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
            result.end(), {deltaLine, deltaStartChar, length, convertTokenType(token.tokenType), static_cast<size_t>(token.tokenModifiers)});

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
    auto module = frontend.moduleResolver.getModule(moduleName);
    if (!sourceModule || !module)
        return std::nullopt;

    auto tokens = getSemanticTokens(module, sourceModule);
    return packTokens(tokens);
}

std::optional<std::vector<size_t>> LanguageServer::semanticTokens(const lsp::SemanticTokensParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->semanticTokens(params);
}