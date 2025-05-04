#pragma once

#include <string>
#include <vector>
#include <map>

#include "Luau/Ast.h"
#include "LSP/LuauExt.hpp"
#include "LSP/TextDocument.hpp"
#include "LSP/Utils.hpp"

namespace Luau::LanguageServer::AutoImports
{
struct FindImportsVisitor : public Luau::AstVisitor
{
private:
    std::optional<size_t> previousRequireLine = std::nullopt;

public:
    std::optional<size_t> firstRequireLine = std::nullopt;
    std::vector<std::map<std::string, Luau::AstStatLocal*>> requiresMap{{}};

    virtual bool handleLocal(Luau::AstStatLocal* local, Luau::AstLocal* localName, Luau::AstExpr* expr, unsigned int startLine, unsigned int endLine)
    {
        return false;
    }

    [[nodiscard]] virtual size_t getMinimumRequireLine() const
    {
        return 0;
    }

    [[nodiscard]] virtual bool shouldPrependNewline(size_t lineNumber) const
    {
        return false;
    }

    bool containsRequire(const std::string& module) const;
    bool visit(Luau::AstStatLocal* local) override;
    bool visit(Luau::AstStatBlock* block) override;
};

std::string makeValidVariableName(std::string name);
lsp::TextEdit createRequireTextEdit(const std::string& name, const std::string& path, size_t lineNumber, bool prependNewline = false);
lsp::CompletionItem createSuggestRequire(const std::string& name, const std::vector<lsp::TextEdit>& textEdits, const char* sortText,
    const std::string& path, const std::string& requirePath);
size_t computeMinimumLineNumberForRequire(const FindImportsVisitor& importsVisitor, size_t hotCommentsLineNumber);
size_t computeBestLineForRequire(
    const FindImportsVisitor& importsVisitor, const TextDocument& textDocument, const std::string& require, size_t minimumLineNumber);
}