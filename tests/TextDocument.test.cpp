// Source: https://github.com/microsoft/vscode-languageserver-node/blob/main/textDocument/src/test/textdocument.test.ts

#include "doctest.h"
#include "LSP/Protocol.hpp"
#include "LSP/TextDocument.hpp"
#include "LSP/Uri.hpp"

TextDocument newDocument(const std::string& content)
{
    return TextDocument(Uri::parse("file://foo/bar"), "text", 0, content);
}

TEST_SUITE_BEGIN("TextDocument Lines Validation");

TEST_CASE("Empty content")
{
    auto str = "";
    auto document = newDocument(str);
    CHECK_EQ(document.lineCount(), 1);
    CHECK_EQ(document.offsetAt(lsp::Position{0, 0}), 0);
    CHECK_EQ(document.positionAt(0), lsp::Position{0, 0});
};

TEST_CASE("Single line")
{
    std::string str = "Hello World";
    auto document = newDocument(str);
    CHECK_EQ(document.lineCount(), 1);

    for (size_t i = 0; i < str.length(); i++)
    {
        CHECK_EQ(document.offsetAt(lsp::Position{0, i}), i);
        CHECK_EQ(document.positionAt(i), lsp::Position{0, i});
    }
};

TEST_CASE("Multiple lines")
{
    std::string str = "ABCDE\nFGHIJ\nKLMNO\n";
    auto document = newDocument(str);
    CHECK_EQ(document.lineCount(), 4);

    for (size_t i = 0; i < str.length(); i++)
    {
        auto line = i / 6;
        auto column = i % 6;
        CHECK_EQ(document.offsetAt(lsp::Position{line, column}), i);
        CHECK_EQ(document.positionAt(i), lsp::Position{line, column});
    }

    CHECK_EQ(document.offsetAt(lsp::Position{3, 0}), 18);
    CHECK_EQ(document.offsetAt(lsp::Position{3, 1}), 18);
    CHECK_EQ(document.positionAt(18), lsp::Position{3, 0});
    CHECK_EQ(document.positionAt(19), lsp::Position{3, 0});
};

TEST_CASE("Starts with new-line")
{
    std::string str = "\nABCDE";
    auto document = newDocument(str);
    CHECK_EQ(document.lineCount(), 2);

    CHECK_EQ(document.positionAt(0), lsp::Position{0, 0});
    CHECK_EQ(document.positionAt(1), lsp::Position{1, 0});
    CHECK_EQ(document.positionAt(6), lsp::Position{1, 5});
};

TEST_CASE("New line characters")
{
    auto str = "ABCDE\rFGHIJ";
    CHECK_EQ(newDocument(str).lineCount(), 2);

    str = "ABCDE\nFGHIJ";
    CHECK_EQ(newDocument(str).lineCount(), 2);

    str = "ABCDE\r\nFGHIJ";
    CHECK_EQ(newDocument(str).lineCount(), 2);

    str = "ABCDE\n\nFGHIJ";
    CHECK_EQ(newDocument(str).lineCount(), 3);

    str = "ABCDE\r\rFGHIJ";
    CHECK_EQ(newDocument(str).lineCount(), 3);

    str = "ABCDE\n\rFGHIJ";
    CHECK_EQ(newDocument(str).lineCount(), 3);
};

TEST_CASE("getText(Range)")
{
    std::string str = "12345\n12345\n12345";
    auto document = newDocument(str);
    CHECK_EQ(document.getText(), str);
    // CHECK_EQ(document.getText(lsp::Range{{-1, 0}, {0, 5}}), "12345"); // DEVIATION: We don't accept negative positions
    CHECK_EQ(document.getText(lsp::Range{{0, 0}, {0, 5}}), "12345");
    CHECK_EQ(document.getText(lsp::Range{{0, 4}, {1, 1}}), "5\n1");
    CHECK_EQ(document.getText(lsp::Range{{0, 4}, {2, 1}}), "5\n12345\n1");
    CHECK_EQ(document.getText(lsp::Range{{0, 4}, {3, 1}}), "5\n12345\n12345");
    CHECK_EQ(document.getText(lsp::Range{{0, 0}, {3, 5}}), str);
};

TEST_CASE("Invalid inputs")
{
    std::string str = "Hello World";
    auto document = newDocument(str);

    // invalid position
    CHECK_EQ(document.offsetAt(lsp::Position{0, str.length()}), str.length());
    CHECK_EQ(document.offsetAt(lsp::Position{0, str.length() + 3}), str.length());
    CHECK_EQ(document.offsetAt(lsp::Position{2, 3}), str.length());
    // DEVIATION: We don't accept negative positions
    // CHECK_EQ(document.offsetAt(lsp::Position{-1, 3}), 0);
    // CHECK_EQ(document.offsetAt(lsp::Position{0, -3}), 0);
    // CHECK_EQ(document.offsetAt(lsp::Position{1, -3}), str.length());

    // invalid offsets
    // CHECK_EQ(document.positionAt(-1), lsp::Position{0, 0});
    CHECK_EQ(document.positionAt(str.length()), lsp::Position{0, str.length()});
    CHECK_EQ(document.positionAt(str.length() + 3), lsp::Position{0, str.length()});
};

// https://github.com/llvm/llvm-project/blob/main/clang-tools-extra/clangd/unittests/SourceCodeTests.cpp
TEST_CASE("lspLength")
{
    CHECK_EQ(lspLength(""), 0UL);
    CHECK_EQ(lspLength("ascii"), 5UL);
    // BMP
    CHECK_EQ(lspLength("↓"), 1UL);
    CHECK_EQ(lspLength("¥"), 1UL);
    // astral
    CHECK_EQ(lspLength("😂"), 2UL);

    // WithContextValue UTF8(kCurrentOffsetEncoding, OffsetEncoding::UTF8);
    // EXPECT_EQ(lspLength(""), 0UL);
    // EXPECT_EQ(lspLength("ascii"), 5UL);
    // // BMP
    // EXPECT_EQ(lspLength("↓"), 3UL);
    // EXPECT_EQ(lspLength("¥"), 2UL);
    // // astral
    // EXPECT_EQ(lspLength("😂"), 4UL);

    // WithContextValue UTF32(kCurrentOffsetEncoding, OffsetEncoding::UTF32);
    // EXPECT_EQ(lspLength(""), 0UL);
    // EXPECT_EQ(lspLength("ascii"), 5UL);
    // // BMP
    // EXPECT_EQ(lspLength("↓"), 1UL);
    // EXPECT_EQ(lspLength("¥"), 1UL);
    // // astral
    // EXPECT_EQ(lspLength("😂"), 1UL);
}

TEST_CASE("PositionToOffset")
{
    auto document = newDocument(R"(0:0 = 0
1:0 → 8
2:0 🡆 18)");

    // DEVIATION: we do not accept negative positions
    // line out of bounds
    // CHECK_EQ(document.offsetAt(lsp::Position{-1, 2}), 0);

    // first line
    // CHECK_EQ(document.offsetAt(lsp::Position{0, -1}), 0); // out of range
    CHECK_EQ(document.offsetAt(lsp::Position{0, 0}), 0); // first character
    CHECK_EQ(document.offsetAt(lsp::Position{0, 3}), 3); // middle character
    CHECK_EQ(document.offsetAt(lsp::Position{0, 6}), 6); // last character
    CHECK_EQ(document.offsetAt(lsp::Position{0, 7}), 7); // the newline itself
    CHECK_EQ(document.offsetAt(lsp::Position{0, 7}), 7);
    CHECK_EQ(document.offsetAt(lsp::Position{0, 8}), 8); // out of range
    // middle line
    // CHECK_EQ(document.offsetAt(lsp::Position{1, -1}), 0); // out of range
    CHECK_EQ(document.offsetAt(lsp::Position{1, 0}), 8);  // first character
    CHECK_EQ(document.offsetAt(lsp::Position{1, 3}), 11); // middle character
    CHECK_EQ(document.offsetAt(lsp::Position{1, 6}), 16); // last character
    CHECK_EQ(document.offsetAt(lsp::Position{1, 7}), 17); // the newline itself
    CHECK_EQ(document.offsetAt(lsp::Position{1, 8}), 18); // out of range
    // last line
    // CHECK_EQ(document.offsetAt(lsp::Position{2, -1}), 0); // out of range
    CHECK_EQ(document.offsetAt(lsp::Position{2, 0}), 18);  // first character
    CHECK_EQ(document.offsetAt(lsp::Position{2, 3}), 21);  // middle character
    CHECK_EQ(document.offsetAt(lsp::Position{2, 5}), 26);  // middle of surrogate pair
    CHECK_EQ(document.offsetAt(lsp::Position{2, 6}), 26);  // end of surrogate pair
    CHECK_EQ(document.offsetAt(lsp::Position{2, 8}), 28);  // last character
    CHECK_EQ(document.offsetAt(lsp::Position{2, 9}), 29);  // EOF
    CHECK_EQ(document.offsetAt(lsp::Position{2, 10}), 29); // out of range
    // line out of bounds
    CHECK_EQ(document.offsetAt(lsp::Position{3, 0}), 29);
    CHECK_EQ(document.offsetAt(lsp::Position{3, 1}), 29);
};

TEST_CASE("OffsetToPosition")
{
    auto document = newDocument(R"(0:0 = 0
1:0 → 8
2:0 🡆 18)");
    CHECK_EQ(document.positionAt(0), lsp::Position{0, 0});  // "start of file"
    CHECK_EQ(document.positionAt(3), lsp::Position{0, 3});  // "in first line"
    CHECK_EQ(document.positionAt(6), lsp::Position{0, 6});  // "end of first line"
    CHECK_EQ(document.positionAt(7), lsp::Position{0, 7});  // "first newline"
    CHECK_EQ(document.positionAt(8), lsp::Position{1, 0});  // "start of second line"
    CHECK_EQ(document.positionAt(12), lsp::Position{1, 4}); // "before BMP char"
    CHECK_EQ(document.positionAt(13), lsp::Position{1, 5}); // "in BMP char"
    CHECK_EQ(document.positionAt(15), lsp::Position{1, 5}); // "after BMP char"
    CHECK_EQ(document.positionAt(16), lsp::Position{1, 6}); // "end of second line"
    CHECK_EQ(document.positionAt(17), lsp::Position{1, 7}); // "second newline"
    CHECK_EQ(document.positionAt(18), lsp::Position{2, 0}); // "start of last line"
    CHECK_EQ(document.positionAt(21), lsp::Position{2, 3}); // "in last line"
    CHECK_EQ(document.positionAt(22), lsp::Position{2, 4}); // "before astral char"
    CHECK_EQ(document.positionAt(24), lsp::Position{2, 6}); // "in astral char"
    CHECK_EQ(document.positionAt(26), lsp::Position{2, 6}); // "after astral char"
    CHECK_EQ(document.positionAt(28), lsp::Position{2, 8}); // "end of last line"
    CHECK_EQ(document.positionAt(29), lsp::Position{2, 9}); // "EOF"
    CHECK_EQ(document.positionAt(30), lsp::Position{2, 9}); // "out of bounds"

    // // Codepoints are similar, except near astral characters.
    // WithContextValue UTF32(kCurrentOffsetEncoding, OffsetEncoding::UTF32);
    // CHECK_EQ(document.positionAt(0), lsp::Position{0, 0}) << "start of file";
    // CHECK_EQ(document.positionAt(3), lsp::Position{0, 3}) << "in first line";
    // CHECK_EQ(document.positionAt(6), lsp::Position{0, 6}) << "end of first line";
    // CHECK_EQ(document.positionAt(7), lsp::Position{0, 7}) << "first newline";
    // CHECK_EQ(document.positionAt(8), lsp::Position{1, 0}) << "start of second line";
    // CHECK_EQ(document.positionAt(12), lsp::Position{1, 4}) << "before BMP char";
    // CHECK_EQ(document.positionAt(13), lsp::Position{1, 5}) << "in BMP char";
    // CHECK_EQ(document.positionAt(15), lsp::Position{1, 5}) << "after BMP char";
    // CHECK_EQ(document.positionAt(16), lsp::Position{1, 6}) << "end of second line";
    // CHECK_EQ(document.positionAt(17), lsp::Position{1, 7}) << "second newline";
    // CHECK_EQ(document.positionAt(18), lsp::Position{2, 0}) << "start of last line";
    // CHECK_EQ(document.positionAt(21), lsp::Position{2, 3}) << "in last line";
    // CHECK_EQ(document.positionAt(22), lsp::Position{2, 4}) << "before astral char";
    // CHECK_EQ(document.positionAt(24), lsp::Position{2, 5}) << "in astral char";
    // CHECK_EQ(document.positionAt(26), lsp::Position{2, 5}) << "after astral char";
    // CHECK_EQ(document.positionAt(28), lsp::Position{2, 7}) << "end of last line";
    // CHECK_EQ(document.positionAt(29), lsp::Position{2, 8}) << "EOF";
    // CHECK_EQ(document.positionAt(30), lsp::Position{2, 8}) << "out of bounds";

    // WithContextValue UTF8(kCurrentOffsetEncoding, OffsetEncoding::UTF8);
    // for (Line L : FileLines)
    // {
    //     for (unsigned I = 0; I <= L.Length; ++I)
    //         CHECK_EQ(document.positionAt(L.Offset + I), Pos(L.Number, I));
    // }
    // CHECK_EQ(document.positionAt(30), lsp::Position{2, 11}) << "out of bounds";
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("Text Document Full Updates");

TEST_CASE("One full update")
{
    auto document = newDocument("abc123");
    document.update({{std::nullopt, "efg456"}}, 1);
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.getText(), "efg456");
};

TEST_CASE("Several full content updates")
{
    auto document = newDocument("abc123");
    document.update({{std::nullopt, "hello"}, {std::nullopt, "world"}}, 2);
    CHECK_EQ(document.version(), 2);
    CHECK_EQ(document.getText(), "world");
};

TEST_SUITE_END();

TEST_SUITE_BEGIN("Text Document Incremental Updates");

// assumes that only '\n' is used
void assertValidLineNumbers(TextDocument& doc)
{
    auto text = doc.getText();
    auto expectedLineNumber = 0;
    for (auto i = 0; i < text.length(); i++)
    {
        CHECK_EQ(doc.positionAt(i).line, expectedLineNumber);
        auto ch = text[i];
        if (ch == '\n')
        {
            expectedLineNumber++;
        }
    }
    CHECK_EQ(doc.positionAt(text.length()).line, expectedLineNumber);
}

lsp::Range rangeForSubstring(TextDocument& document, const std::string& subText)
{
    auto index = document.getText().find(subText);
    auto a = document.positionAt(index);
    auto b = document.positionAt(index + subText.length());
    return lsp::Range{a, b};
}

lsp::Range rangeAfterSubstring(TextDocument& document, const std::string& subText)
{
    auto index = document.getText().find(subText);
    auto pos = document.positionAt(index + subText.length());
    return lsp::Range{pos, pos};
}

TEST_CASE("Incrementally removing content")
{
    auto document = newDocument("function abc() {\n  console.log(\"hello, world!\");\n}");
    CHECK_EQ(document.lineCount(), 3);
    assertValidLineNumbers(document);
    document.update({{rangeForSubstring(document, "hello, world!"), ""}}, 1);
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.getText(), "function abc() {\n  console.log(\"\");\n}");
    CHECK_EQ(document.lineCount(), 3);
    assertValidLineNumbers(document);
};

TEST_CASE("Incrementally removing multi-line content")
{
    auto document = newDocument("function abc() {\n  foo();\n  bar();\n  \n}");
    CHECK_EQ(document.lineCount(), 5);
    assertValidLineNumbers(document);
    document.update({{rangeForSubstring(document, "  foo();\n  bar();\n"), ""}}, 1);
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.getText(), "function abc() {\n  \n}");
    CHECK_EQ(document.lineCount(), 3);
    assertValidLineNumbers(document);
};

TEST_CASE("Incrementally removing multi-line content 2")
{
    auto document = newDocument("function abc() {\n  foo();\n  bar();\n  \n}");
    CHECK_EQ(document.lineCount(), 5);
    assertValidLineNumbers(document);
    document.update({{rangeForSubstring(document, "foo();\n  bar();"), ""}}, 1);
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.getText(), "function abc() {\n  \n  \n}");
    CHECK_EQ(document.lineCount(), 4);
    assertValidLineNumbers(document);
};

TEST_CASE("Incrementally adding content")
{
    auto document = newDocument("function abc() {\n  console.log(\"hello\");\n}");
    CHECK_EQ(document.lineCount(), 3);
    assertValidLineNumbers(document);
    document.update({{rangeAfterSubstring(document, "hello"), ", world!"}}, 1);
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.getText(), "function abc() {\n  console.log(\"hello, world!\");\n}");
    CHECK_EQ(document.lineCount(), 3);
    assertValidLineNumbers(document);
};

TEST_CASE("Incrementally adding multi-line content")
{
    auto document = newDocument("function abc() {\n  while (true) {\n    foo();\n  };\n}");
    CHECK_EQ(document.lineCount(), 5);
    assertValidLineNumbers(document);
    document.update({{rangeAfterSubstring(document, "foo();"), "\n    bar();"}}, 1);
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.getText(), "function abc() {\n  while (true) {\n    foo();\n    bar();\n  };\n}");
    CHECK_EQ(document.lineCount(), 6);
    assertValidLineNumbers(document);
};

TEST_CASE("Incrementally replacing single-line content, more chars")
{
    auto document = newDocument("function abc() {\n  console.log(\"hello, world!\");\n}");
    CHECK_EQ(document.lineCount(), 3);
    assertValidLineNumbers(document);
    document.update({{rangeForSubstring(document, "hello, world!"), "hello, test case!!!"}}, 1);
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.getText(), "function abc() {\n  console.log(\"hello, test case!!!\");\n}");
    CHECK_EQ(document.lineCount(), 3);
    assertValidLineNumbers(document);
};

TEST_CASE("Incrementally replacing single-line content, less chars")
{
    auto document = newDocument("function abc() {\n  console.log(\"hello, world!\");\n}");
    CHECK_EQ(document.lineCount(), 3);
    assertValidLineNumbers(document);
    document.update({{rangeForSubstring(document, "hello, world!"), "hey"}}, 1);
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.getText(), "function abc() {\n  console.log(\"hey\");\n}");
    CHECK_EQ(document.lineCount(), 3);
    assertValidLineNumbers(document);
};

TEST_CASE("Incrementally replacing single-line content, same num of chars")
{
    auto document = newDocument("function abc() {\n  console.log(\"hello, world!\");\n}");
    CHECK_EQ(document.lineCount(), 3);
    assertValidLineNumbers(document);
    document.update({{rangeForSubstring(document, "hello, world!"), "world, hello!"}}, 1);
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.getText(), "function abc() {\n  console.log(\"world, hello!\");\n}");
    CHECK_EQ(document.lineCount(), 3);
    assertValidLineNumbers(document);
};

TEST_CASE("Incrementally replacing multi-line content, more lines")
{
    auto document = newDocument("function abc() {\n  console.log(\"hello, world!\");\n}");
    CHECK_EQ(document.lineCount(), 3);
    assertValidLineNumbers(document);
    document.update({{rangeForSubstring(document, "function abc() {"), "\n//hello\nfunction d(){"}}, 1);
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.getText(), "\n//hello\nfunction d(){\n  console.log(\"hello, world!\");\n}");
    CHECK_EQ(document.lineCount(), 5);
    assertValidLineNumbers(document);
};

TEST_CASE("Incrementally replacing multi-line content, less lines")
{
    auto document = newDocument("a1\nb1\na2\nb2\na3\nb3\na4\nb4\n");
    CHECK_EQ(document.lineCount(), 9);
    assertValidLineNumbers(document);
    document.update({{rangeForSubstring(document, "\na3\nb3\na4\nb4\n"), "xx\nyy"}}, 1);
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.getText(), "a1\nb1\na2\nb2xx\nyy");
    CHECK_EQ(document.lineCount(), 5);
    assertValidLineNumbers(document);
};

TEST_CASE("Incrementally replacing multi-line content, same num of lines and chars")
{
    auto document = newDocument("a1\nb1\na2\nb2\na3\nb3\na4\nb4\n");
    CHECK_EQ(document.lineCount(), 9);
    assertValidLineNumbers(document);
    document.update({{rangeForSubstring(document, "a2\nb2\na3"), "\nxx1\nxx2"}}, 1);
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.getText(), "a1\nb1\n\nxx1\nxx2\nb3\na4\nb4\n");
    CHECK_EQ(document.lineCount(), 9);
    assertValidLineNumbers(document);
};

TEST_CASE("Incrementally replacing multi-line content, same num of lines but diff chars")
{
    auto document = newDocument("a1\nb1\na2\nb2\na3\nb3\na4\nb4\n");
    CHECK_EQ(document.lineCount(), 9);
    assertValidLineNumbers(document);
    document.update({{rangeForSubstring(document, "a2\nb2\na3"), "\ny\n"}}, 1);
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.getText(), "a1\nb1\n\ny\n\nb3\na4\nb4\n");
    CHECK_EQ(document.lineCount(), 9);
    assertValidLineNumbers(document);
};

TEST_CASE("Incrementally replacing multi-line content, huge number of lines")
{
    auto document = newDocument("a1\ncc\nb1");
    CHECK_EQ(document.lineCount(), 3);
    assertValidLineNumbers(document);
    std::string text = ""; // a string with 19999 `\n`
    for (int i = 0; i < 19999; i++)
    {
        text += "\ndd";
    }
    document.update({{rangeForSubstring(document, "\ncc"), text}}, 1);
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.getText(), "a1" + text + "\nb1");
    CHECK_EQ(document.lineCount(), 20001);
    assertValidLineNumbers(document);
};

TEST_CASE("Several incremental content changes")
{
    auto document = newDocument("function abc() {\n  console.log(\"hello, world!\");\n}");
    document.update(
        {
            {lsp::Range{{0, 12}, {0, 12}}, "defg"},
            {lsp::Range{{1, 15}, {1, 28}}, "hello, test case!!!"},
            {lsp::Range{{0, 16}, {0, 16}}, "hij"},
        },
        1);
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.getText(), "function abcdefghij() {\n  console.log(\"hello, test case!!!\");\n}");
    assertValidLineNumbers(document);
};

TEST_CASE("Basic append")
{
    auto document = newDocument("foooo\nbar\nbaz");

    CHECK_EQ(document.offsetAt(lsp::Position{2, 0}), 10);

    document.update({{lsp::Range{{1, 3}, {1, 3}}, " some extra content"}}, 1);
    CHECK_EQ(document.getText(), "foooo\nbar some extra content\nbaz");
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.offsetAt(lsp::Position{2, 0}), 29);
    assertValidLineNumbers(document);
};

TEST_CASE("Multi-line append")
{
    auto document = newDocument("foooo\nbar\nbaz");

    CHECK_EQ(document.offsetAt(lsp::Position{2, 0}), 10);

    document.update({{lsp::Range{{1, 3}, {1, 3}}, " some extra\ncontent"}}, 1);
    CHECK_EQ(document.getText(), "foooo\nbar some extra\ncontent\nbaz");
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.offsetAt(lsp::Position{3, 0}), 29);
    CHECK_EQ(document.lineCount(), 4);
    assertValidLineNumbers(document);
};

TEST_CASE("Basic delete")
{
    auto document = newDocument("foooo\nbar\nbaz");

    CHECK_EQ(document.offsetAt(lsp::Position{2, 0}), 10);

    document.update({{lsp::Range{{1, 0}, {1, 3}}, ""}}, 1);
    CHECK_EQ(document.getText(), "foooo\n\nbaz");
    CHECK_EQ(document.version(), 1);
    CHECK_EQ(document.offsetAt(lsp::Position{2, 0}), 7);
    assertValidLineNumbers(document);
};

TEST_CASE("Multi-line delete")
{
    auto lm = newDocument("foooo\nbar\nbaz");

    CHECK_EQ(lm.offsetAt(lsp::Position{2, 0}), 10);

    lm.update({{lsp::Range{{0, 5}, {1, 3}}, ""}}, 1);
    CHECK_EQ(lm.getText(), "foooo\nbaz");
    CHECK_EQ(lm.version(), 1);
    CHECK_EQ(lm.offsetAt(lsp::Position{1, 0}), 6);
    assertValidLineNumbers(lm);
};

TEST_CASE("Single character replace")
{
    auto document = newDocument("foooo\nbar\nbaz");

    CHECK_EQ(document.offsetAt(lsp::Position{2, 0}), 10);

    document.update({{lsp::Range{{1, 2}, {1, 3}}, "z"}}, 2);
    CHECK_EQ(document.getText(), "foooo\nbaz\nbaz");
    CHECK_EQ(document.version(), 2);
    CHECK_EQ(document.offsetAt(lsp::Position{2, 0}), 10);
    assertValidLineNumbers(document);
};

TEST_CASE("Multi-character replace")
{
    auto lm = newDocument("foo\nbar");

    CHECK_EQ(lm.offsetAt(lsp::Position{1, 0}), 4);

    lm.update({{lsp::Range{{1, 0}, {1, 3}}, "foobar"}}, 1);
    CHECK_EQ(lm.getText(), "foo\nfoobar");
    CHECK_EQ(lm.version(), 1);
    CHECK_EQ(lm.offsetAt(lsp::Position{1, 0}), 4);
    assertValidLineNumbers(lm);
};

TEST_CASE("Invalid update ranges")
{
    // DEVIATION: We do not accept negative positions
    // Before the document starts -> before the document starts
    // auto document = newDocument("foo\nbar");
    // document.update([ {"abc123", Ranges.create(-2, 0, -1, 3)} ], 2);
    // CHECK_EQ(document.getText(), "abc123foo\nbar");
    // CHECK_EQ(document.version(), 2);
    // assertValidLineNumbers(document);

    // DEVIATION: We do not accept negative positions
    // // Before the document starts -> the middle of document
    // document = newDocument("foo\nbar");
    // document.update([ {"foobar", Ranges.create(-1, 0, 0, 3)} ], 2);
    // CHECK_EQ(document.getText(), "foobar\nbar");
    // CHECK_EQ(document.version(), 2);
    // CHECK_EQ(document.offsetAt(lsp::Position{1, 0}), 7);
    // assertValidLineNumbers(document);

    // The middle of document -> after the document ends
    auto document = newDocument("foo\nbar");
    document.update({{lsp::Range{{1, 0}, {1, 10}}, "foobar"}}, 2);
    CHECK_EQ(document.getText(), "foo\nfoobar");
    CHECK_EQ(document.version(), 2);
    CHECK_EQ(document.offsetAt(lsp::Position{1, 1000}), 10);
    assertValidLineNumbers(document);

    // After the document ends -> after the document ends
    document = newDocument("foo\nbar");
    document.update({{lsp::Range{{3, 0}, {6, 10}}, "abc123"}}, 2);
    CHECK_EQ(document.getText(), "foo\nbarabc123");
    CHECK_EQ(document.version(), 2);
    assertValidLineNumbers(document);

    // DEVIATION: We do not accept negative positions
    // Before the document starts -> after the document ends
    // document = newDocument("foo\nbar");
    // document.update([ {"entirely new content", Ranges.create(-1, 1, 2, 10000)} ], 2);
    // CHECK_EQ(document.getText(), "entirely new content");
    // CHECK_EQ(document.version(), 2);
    // CHECK_EQ(document.lineCount(), 1);
    // assertValidLineNumbers(document);
};

TEST_SUITE_END();