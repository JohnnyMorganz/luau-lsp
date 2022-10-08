#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/Protocol.hpp"
#include "LSP/LuauExt.hpp"

struct DocumentSymbolsVisitor : public Luau::AstVisitor
{
    const TextDocument* textDocument;
    std::vector<lsp::DocumentSymbol> symbols;
    lsp::DocumentSymbol* parent = nullptr;

    explicit DocumentSymbolsVisitor(const TextDocument* textDocument)
        : textDocument(textDocument)
    {
    }

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
        symbol.range = {textDocument->convertPosition(local->location.begin), textDocument->convertPosition(local->location.end)};
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
        symbol.range = {textDocument->convertPosition(function->location.begin), textDocument->convertPosition(function->location.end)};
        symbol.selectionRange = {
            textDocument->convertPosition(function->name->location.begin), textDocument->convertPosition(function->name->location.end)};
        trim(symbol.name);
        visitFunction(function->func, symbol);
        addSymbol(symbol);
        return false;
    }

    bool visit(Luau::AstStatLocalFunction* func) override
    {
        lsp::DocumentSymbol symbol;
        symbol.name = Luau::toString(func->name);
        symbol.kind = lsp::SymbolKind::Function;
        symbol.range = {textDocument->convertPosition(func->location.begin), textDocument->convertPosition(func->location.end)};
        symbol.selectionRange = {textDocument->convertPosition(func->name->location.begin), textDocument->convertPosition(func->name->location.end)};
        visitFunction(func->func, symbol);
        addSymbol(symbol);
        return false;
    }

    bool visit(Luau::AstStatTypeAlias* alias) override
    {
        lsp::DocumentSymbol symbol;
        symbol.name = alias->name.value;
        symbol.kind = lsp::SymbolKind::Interface;
        symbol.range = {textDocument->convertPosition(alias->location.begin), textDocument->convertPosition(alias->location.end)};
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
            symbol.range = {textDocument->convertPosition(func->varargLocation.begin), textDocument->convertPosition(func->varargLocation.end)};
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
    auto textDocument = fileResolver.getTextDocument(moduleName);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + moduleName);

    // Run the type checker to ensure we are up to date
    if (frontend.isDirty(moduleName))
        frontend.check(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return std::nullopt;

    DocumentSymbolsVisitor visitor{textDocument};
    visitor.visit(sourceModule->root);
    return visitor.symbols;
}

std::optional<std::vector<lsp::DocumentSymbol>> LanguageServer::documentSymbol(const lsp::DocumentSymbolParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->documentSymbol(params);
}