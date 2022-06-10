#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/Protocol.hpp"
#include "LSP/LuauExt.hpp"

struct DocumentSymbolsVisitor : public Luau::AstVisitor
{
    std::vector<lsp::DocumentSymbol> symbols;
    lsp::DocumentSymbol* parent = nullptr;

    void addSymbol(const lsp::DocumentSymbol& symbol)
    {
        if (parent)
            parent->children.push_back(symbol);
        else
            symbols.push_back(symbol);
    }

    void createLocalSymbol(Luau::AstLocal* local)
    {
        lsp::DocumentSymbol symbol;
        symbol.name = local->name.value;
        symbol.kind = lsp::SymbolKind::Variable;
        symbol.range = {convertPosition(local->location.begin), convertPosition(local->location.end)};
        symbol.selectionRange = symbol.range;
        addSymbol(symbol);
    }

    bool visit(Luau::AstStatLocal* local) override
    {
        for (size_t i = 0; i < local->vars.size; ++i)
        {
            createLocalSymbol(local->vars.data[i]);
            // TODO: if the value assigned is a table, should we include its properties?
        }
        return false;
    }

    bool visit(Luau::AstStatFunction* function) override
    {
        lsp::DocumentSymbol symbol;
        symbol.name = Luau::toString(function->name);
        symbol.kind = lsp::SymbolKind::Function;
        symbol.range = {convertPosition(function->location.begin), convertPosition(function->location.end)};
        symbol.selectionRange = {convertPosition(function->name->location.begin), convertPosition(function->name->location.end)};
        visitFunction(function->func, symbol);
        addSymbol(symbol);
        return false;
    }

    bool visit(Luau::AstStatLocalFunction* func) override
    {
        lsp::DocumentSymbol symbol;
        symbol.name = Luau::toString(func->name);
        symbol.kind = lsp::SymbolKind::Function;
        symbol.range = {convertPosition(func->location.begin), convertPosition(func->location.end)};
        symbol.selectionRange = {convertPosition(func->name->location.begin), convertPosition(func->name->location.end)};
        visitFunction(func->func, symbol);
        addSymbol(symbol);
        return false;
    }

    bool visit(Luau::AstStatTypeAlias* alias) override
    {
        lsp::DocumentSymbol symbol;
        symbol.name = alias->name.value;
        symbol.kind = lsp::SymbolKind::Interface;
        symbol.range = {convertPosition(alias->location.begin), convertPosition(alias->location.end)};
        symbol.selectionRange = symbol.range;
        addSymbol(symbol);
        return false;
    }

    void visitFunction(Luau::AstExprFunction* func, lsp::DocumentSymbol& symbol)
    {
        auto oldParent = parent;
        parent = &symbol;

        // Create parameter symbols + build function detail
        std::string detail = "function (";
        bool comma = false;
        for (auto* arg : func->args)
        {
            createLocalSymbol(arg);
            if (comma)
                detail += ", ";
            detail += arg->name.value;
            comma = true;
        }
        if (func->vararg)
        {
            lsp::DocumentSymbol symbol;
            symbol.name = "...";
            symbol.kind = lsp::SymbolKind::Variable;
            symbol.range = {convertPosition(func->varargLocation.begin), convertPosition(func->varargLocation.end)};
            addSymbol(symbol);

            if (comma)
                detail += ", ";
            detail += "...";
        }
        detail += ")";
        symbol.detail = detail;

        // Create symbols for body
        for (Luau::AstStat* stat : func->body->body)
        {
            stat->visit(this);
        }

        parent = oldParent;
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

std::optional<std::vector<lsp::DocumentSymbol>> WorkspaceFolder::documentSymbol(const lsp::DocumentSymbolParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);

    // Run the type checker to ensure we are up to date
    if (frontend.isDirty(moduleName))
        frontend.check(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return std::nullopt;

    DocumentSymbolsVisitor visitor;
    visitor.visit(sourceModule->root);
    return visitor.symbols;
}

Response LanguageServer::documentSymbol(const lsp::DocumentSymbolParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    auto result = workspace->documentSymbol(params);
    if (result)
        return *result;
    return nullptr;
}