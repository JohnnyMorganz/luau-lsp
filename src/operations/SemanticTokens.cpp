#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/Protocol.hpp"
#include "LSP/LuauExt.hpp"
#include "LSP/SemanticTokens.hpp"

static lsp::SemanticTokenTypes inferTokenType(Luau::TypeId* ty, lsp::SemanticTokenTypes base)
{
    if (!ty)
        return base;

    auto followedTy = Luau::follow(*ty);

    if (Luau::get<Luau::FunctionTypeVar>(followedTy))
    {
        return lsp::SemanticTokenTypes::Function;
    }
    else if (auto intersection = Luau::get<Luau::IntersectionTypeVar>(followedTy))
    {
        if (Luau::isOverloadedFunction(followedTy))
        {
            return lsp::SemanticTokenTypes::Function;
        }
    }
    else if (auto ttv = Luau::get<Luau::TableTypeVar>(followedTy))
    {
        if (ttv->name && ttv->name == "RBXScriptSignal")
        {
            return lsp::SemanticTokenTypes::Event;
        }
    }

    return base;
}

struct SemanticTokensVisitor : public Luau::AstVisitor
{
    Luau::ModulePtr module;
    std::vector<SemanticToken> tokens;

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
        // HACK: The location information provided only gives location for the whole reference
        // but we do not want to highlight punctuation inside of it (Module.Type<Param> should not highlight . < >)
        // So we use the start position and consider the end positions separately.
        // Here, we make the assumption that there is no comments or newlines present in between the punctuation

        auto startPosition = ref->location.begin;
        // Highlight prefix if exists
        if (ref->prefix)
        {
            Luau::Position endPosition{startPosition.line, startPosition.column + strlen(ref->prefix->value)};
            tokens.emplace_back(SemanticToken{startPosition, endPosition, lsp::SemanticTokenTypes::Namespace, lsp::SemanticTokenModifiers::None});
            startPosition = {endPosition.line, endPosition.column + 1};
        }

        // Highlight name
        Luau::Position endPosition{startPosition.line, startPosition.column + strlen(ref->name.value)};
        tokens.emplace_back(SemanticToken{startPosition, endPosition, lsp::SemanticTokenTypes::Type, lsp::SemanticTokenModifiers::None});

        // Do not highlight parameters as they will be visited later
        return true;
    }

    bool visit(Luau::AstStatLocal* local) override
    {
        auto scope = Luau::findScopeAtPosition(*module, local->location.begin);
        if (!scope)
            return true;

        for (auto var : local->vars)
        {
            auto ty = scope->lookup(var);
            if (ty)
            {
                auto followedTy = Luau::follow(*ty);
                auto type = inferTokenType(&followedTy, lsp::SemanticTokenTypes::Variable);
                tokens.emplace_back(SemanticToken{var->location.begin, var->location.end, type, lsp::SemanticTokenModifiers::None});
            }
        }

        return true;
    }

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
        }
        return true;
    }

    bool visit(Luau::AstExprLocal* local) override
    {
        auto type = inferTokenType(module->astTypes.find(local), lsp::SemanticTokenTypes::Variable);
        tokens.emplace_back(SemanticToken{local->location.begin, local->location.end, type, lsp::SemanticTokenModifiers::None});
        return true;
    }

    // bool visit(Luau::AstExprGlobal* local) override
    // {
    //     auto type = inferTokenType(module->astTypes.find(local), lsp::SemanticTokenTypes::Namespace);
    //     tokens.emplace_back(SemanticToken{local->location.begin, local->location.end, type, lsp::SemanticTokenModifiers::None});
    //     return true;
    // }

    bool visit(Luau::AstExprIndexName* index) override
    {
        auto parentTy = module->astTypes.find(index->expr);
        if (!parentTy)
            return true;

        auto ty = Luau::follow(*parentTy);
        if (auto prop = lookupProp(ty, std::string(index->index.value)))
        {
            auto type = inferTokenType(&prop->type, lsp::SemanticTokenTypes::Property);
            tokens.emplace_back(SemanticToken{index->indexLocation.begin, index->indexLocation.end, type, lsp::SemanticTokenModifiers::None});
        }

        return true;
    }

    bool visit(Luau::AstExprTable* tbl) override
    {
        for (auto item : tbl->items)
        {
            if (item.kind == Luau::AstExprTable::Item::Kind::Record)
            {
                auto type = inferTokenType(module->astTypes.find(item.value), lsp::SemanticTokenTypes::Property);
                tokens.emplace_back(SemanticToken{item.key->location.begin, item.key->location.end, type, lsp::SemanticTokenModifiers::None});
            }
        }

        return true;
    }

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
        auto length = token.end.column - token.start.column;

        result.insert(
            result.end(), {deltaLine, deltaStartChar, length, convertTokenType(token.tokenType), static_cast<size_t>(token.tokenModifiers)});

        lastLine = line;
        lastChar = startChar;
    }

    return result;
}

std::optional<lsp::SemanticTokens> WorkspaceFolder::semanticTokens(const lsp::SemanticTokensParams& params)
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
    lsp::SemanticTokens result;
    result.data = packTokens(tokens);
    return result;
}

std::optional<lsp::SemanticTokens> LanguageServer::semanticTokens(const lsp::SemanticTokensParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->semanticTokens(params);
}