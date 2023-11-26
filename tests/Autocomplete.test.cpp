#include "doctest.h"
#include "Fixture.h"

static std::pair<std::string, lsp::Position> sourceWithMarker(std::string source)
{
    auto marker = source.find('@');
    REQUIRE(marker != std::string::npos);

    source.replace(marker, 1, "");

    size_t line = 0;
    size_t column = 0;

    for (size_t i = 0; i < source.size(); i++)
    {
        auto ch = source[i];
        if (ch == '\r' || ch == '\n')
        {
            if (ch == '\r' && i + 1 < source.size() && source[i + 1] == '\n')
            {
                i++;
            }
            line += 1;
            column = 0;
        }
        else
            column += 1;

        if (i == marker)
            break;
    }

    return std::make_pair(source, lsp::Position{line, column});
}

lsp::CompletionItem getItem(const std::vector<lsp::CompletionItem>& items, const std::string& label)
{
    for (const auto& item : items)
        if (item.label == label)
            return item;
    REQUIRE_MESSAGE(false, "no item found");
    return {};
}

TEST_SUITE_BEGIN("Autocomplete");

TEST_CASE_FIXTURE(Fixture, "function_autocomplete_has_documentation")
{
    auto [source, marker] = sourceWithMarker(R"(
        --- This is a function documentation comment
        local function foo()
        end

        local x = @
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::CompletionParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.completion(params);
    auto item = getItem(result, "foo");

    REQUIRE(item.documentation);
    CHECK_EQ(item.documentation->kind, lsp::MarkupKind::Markdown);
    trim(item.documentation->value);
    CHECK_EQ(item.documentation->value, "This is a function documentation comment");
}

TEST_SUITE_END();
