#include "TestUtils.h"

std::string applyEdit(const std::string& source, const std::vector<lsp::TextEdit>& edits)
{
    std::string newSource;

    lsp::Position currentPos{0, 0};
    std::optional<lsp::Position> editEndPos = std::nullopt;
    for (const auto& c : source)
    {
        if (c == '\n')
        {
            currentPos.line += 1;
            currentPos.character = 0;
        }
        else
        {
            currentPos.character += 1;
        }

        if (editEndPos)
        {
            if (currentPos == *editEndPos)
                editEndPos = std::nullopt;
        }
        else
            newSource += c;

        for (const auto& edit : edits)
        {
            if (currentPos == edit.range.start)
            {
                newSource += edit.newText;
                editEndPos = edit.range.end;
            }
        }
    }

    return newSource;
}
