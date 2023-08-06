#include "LSP/Workspace.hpp"

struct FoldingRangeVisitor : public Luau::AstVisitor
{
    const lsp::ClientCapabilities& capabilities;
    const TextDocument* textDocument;
    std::vector<lsp::FoldingRange> ranges{};

    explicit FoldingRangeVisitor(const lsp::ClientCapabilities& capabilities, const TextDocument* textDocument)
        : capabilities(capabilities)
        , textDocument(textDocument)
    {
    }

    void addFoldingRange(const Luau::Position& start, const Luau::Position& end, std::optional<lsp::FoldingRangeKind> kind = std::nullopt)
    {
        lsp::FoldingRange range{};

        auto startPosition = textDocument->convertPosition(start);
        auto endPosition = textDocument->convertPosition(end);

        range.kind = kind;
        range.startLine = startPosition.line;
        range.startCharacter = startPosition.character;

        // We want to keep the closing token visible to make it clear what range is being folded
        // but if the client only supports line folding only, we need to use the previous line
        if (capabilities.textDocument && capabilities.textDocument->foldingRange && capabilities.textDocument->foldingRange->lineFoldingOnly &&
            endPosition.line > startPosition.line)
        {
            range.endLine = endPosition.line - 1;
        }
        else
        {
            range.endLine = endPosition.line;
            range.endCharacter = endPosition.character;
        }

        ranges.push_back(range);
    }

    bool visit(Luau::AstExprCall* call) override
    {
        addFoldingRange(call->argLocation.begin, call->argLocation.end);
        return true;
    }

    bool visit(Luau::AstExprTable* table) override
    {
        addFoldingRange(table->location.begin, table->location.end);
        return true;
    }

    bool visit(Luau::AstTypeTable* table) override
    {
        addFoldingRange(table->location.begin, table->location.end);
        return true;
    }

    bool visit(Luau::AstExprFunction* func) override
    {
        // Add folding range for parameters
        if (func->argLocation)
            addFoldingRange(func->argLocation->begin, func->argLocation->end);
        return true;
    }

    bool visit(class Luau::AstType* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatBlock* block) override
    {
        // Create a folding range for this block
        // Ignore the first block
        if (!(block->location.begin.line == 0 && block->location.begin.column == 0))
        {
            addFoldingRange(block->location.begin, block->location.end);
        }

        for (Luau::AstStat* stat : block->body)
        {
            stat->visit(this);
        }

        return false;
    }
};

std::vector<lsp::FoldingRange> WorkspaceFolder::foldingRange(const lsp::FoldingRangeParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());

    frontend.parse(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return {};

    FoldingRangeVisitor visitor{client->capabilities, textDocument};
    visitor.visit(sourceModule->root);

    // Handle comments specificially
    std::vector<Luau::Position> commentRegions;
    for (const auto& comment : sourceModule->commentLocations)
    {
        if (comment.type == Luau::Lexeme::Type::BrokenComment)
            continue;

        auto commentText = textDocument->getText(
            lsp::Range{textDocument->convertPosition(comment.location.begin), textDocument->convertPosition(comment.location.end)});

        if (comment.type == Luau::Lexeme::Type::Comment)
        {
            if (Luau::startsWith(commentText, "--#region"))
            {
                // Create a new region location
                commentRegions.push_back(comment.location.end);
            }
            else if (Luau::startsWith(commentText, "--#endregion"))
            {
                // Close off any open regions
                if (commentRegions.empty())
                    continue;

                auto openRegion = commentRegions.back();
                commentRegions.pop_back();

                visitor.addFoldingRange(openRegion, comment.location.begin, lsp::FoldingRangeKind::Region);
            }
        }
        else if (comment.type == Luau::Lexeme::Type::BlockComment)
        {
            visitor.addFoldingRange(comment.location.begin, comment.location.end, lsp::FoldingRangeKind::Comment);
        }
    }


    return visitor.ranges;
}