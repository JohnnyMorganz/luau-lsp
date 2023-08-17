#include <utility>

#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"

#include "Luau/Transpiler.h"
#include "Protocol/LanguageFeatures.hpp"

struct WorkspaceSymbolsVisitor : public Luau::AstVisitor
{
    const TextDocument* textDocument;
    const std::string query;
    std::vector<lsp::WorkspaceSymbol> symbols{};

    explicit WorkspaceSymbolsVisitor(const TextDocument* textDocument, std::string query)
        : textDocument(textDocument)
        , query(std::move(toLower(query)))
    {
    }

    bool matchesQuery(std::string symbolName)
    {
        return query.empty() || toLower(symbolName).find(query);
    }

    void createLocalSymbol(Luau::AstLocal* local, std::optional<std::string> containerName)
    {
        lsp::WorkspaceSymbol symbol;
        symbol.name = local->name.value;
        if (!matchesQuery(symbol.name))
            return;

        symbol.kind = lsp::SymbolKind::Variable;
        symbol.location = {
            textDocument->uri(), {textDocument->convertPosition(local->location.begin), textDocument->convertPosition(local->location.end)}};
        symbol.containerName = std::move(containerName);
        symbols.push_back(symbol);
    }

    bool visit(Luau::AstStatLocal* local) override
    {
        for (size_t i = 0; i < local->vars.size; ++i)
            createLocalSymbol(local->vars.data[i], std::nullopt);
        return false;
    }

    bool visit(Luau::AstStatFunction* function) override
    {
        lsp::WorkspaceSymbol symbol;
        symbol.name = Luau::toString(function->name);
        trim(symbol.name);
        if (!matchesQuery(symbol.name))
            return true;

        symbol.kind = function->func->self ? lsp::SymbolKind::Method : lsp::SymbolKind::Function;
        symbol.location = {textDocument->uri(),
            {textDocument->convertPosition(function->name->location.begin), textDocument->convertPosition(function->name->location.end)}};


        // TODO: should we create symbols for the function parameters?
        // are they useful at the workspace level?

        symbols.push_back(symbol);
        return true;
    }

    bool visit(Luau::AstStatLocalFunction* function) override
    {
        lsp::WorkspaceSymbol symbol;
        symbol.name = Luau::toString(function->name);
        if (!matchesQuery(symbol.name))
            return true;

        symbol.kind = function->func->self ? lsp::SymbolKind::Method : lsp::SymbolKind::Function;
        symbol.location = {textDocument->uri(),
            {textDocument->convertPosition(function->name->location.begin), textDocument->convertPosition(function->name->location.end)}};
        // TODO: should we create symbols for the function parameters?
        // are they useful at the workspace level?
        symbols.push_back(symbol);
        return true;
    }

    bool visit(Luau::AstStatTypeAlias* alias) override
    {
        lsp::WorkspaceSymbol symbol;
        symbol.name = alias->name.value;
        if (!matchesQuery(symbol.name))
            return false;

        symbol.kind = lsp::SymbolKind::Interface;
        symbol.location = {
            textDocument->uri(), {textDocument->convertPosition(alias->nameLocation.begin), textDocument->convertPosition(alias->nameLocation.end)}};
        symbols.push_back(symbol);
        return false;
    }

    bool visit(Luau::AstStatBlock* block) override
    {
        for (Luau::AstStat* stat : block->body)
            stat->visit(this);

        return false;
    }
};

std::optional<std::vector<lsp::WorkspaceSymbol>> WorkspaceFolder::workspaceSymbol(const lsp::WorkspaceSymbolParams& params)
{
    std::vector<lsp::WorkspaceSymbol> result;

    for (const auto& [moduleName, sourceModule] : frontend.sourceModules)
    {
        frontend.parse(moduleName);

        // Find relevant text document
        if (auto textDocument = fileResolver.getTextDocumentFromModuleName(moduleName))
        {
            WorkspaceSymbolsVisitor visitor{textDocument, params.query};
            visitor.visit(sourceModule->root);
            result.insert(result.end(), std::make_move_iterator(visitor.symbols.begin()), std::make_move_iterator(visitor.symbols.end()));
        }
        else
        {
            if (auto filePath = fileResolver.resolveToRealPath(moduleName))
            {
                if (auto source = fileResolver.readSource(moduleName))
                {
                    auto textDocument = TextDocument{Uri::file(*filePath), "luau", 0, source->source};
                    WorkspaceSymbolsVisitor visitor{&textDocument, params.query};
                    visitor.visit(sourceModule->root);
                    result.insert(result.end(), std::make_move_iterator(visitor.symbols.begin()), std::make_move_iterator(visitor.symbols.end()));
                }
            }
        }
    }

    return result;
}