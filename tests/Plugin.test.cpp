#include "doctest.h"
#include "Fixture.h"
#include "LSP/Uri.hpp"
#include "LSP/Workspace.hpp"
#include "Protocol/Window.hpp"
#include "Plugin/SourceMapping.hpp"
#include "Plugin/PluginTextDocument.hpp"
#include "Plugin/PluginRuntime.hpp"
#include "Plugin/PluginManager.hpp"
#include "Plugin/PluginTypes.hpp"
#include "Luau/NotNull.h"

using namespace Luau::LanguageServer::Plugin;

// Helper to get NotNull workspace from Fixture
inline Luau::NotNull<WorkspaceFolder> getWorkspaceNotNull(Fixture& fixture)
{
    return Luau::NotNull<WorkspaceFolder>{&fixture.workspace};
}

// Helper to extract error messages from TestClient notification queue
inline std::vector<std::string> getLogMessages(const TestClient& client, lsp::MessageType type)
{
    std::vector<std::string> messages;
    for (const auto& [method, params] : client.notificationQueue)
    {
        if (method == "window/logMessage" && params.has_value())
        {
            auto logParams = params->get<lsp::LogMessageParams>();
            if (logParams.type == type)
                messages.push_back(logParams.message);
        }
    }
    return messages;
}

TEST_SUITE_BEGIN("SourceMapping");

TEST_CASE("SourceMapping.fromEdits no edits")
{
    std::string original = "local x = 1";
    std::vector<TextEdit> edits{};

    auto result = SourceMapping::fromEdits(original, edits);

    CHECK(result.transformedSource == original);
    CHECK(result.edits.empty());
}

TEST_CASE("SourceMapping.fromEdits single replacement same length")
{
    std::string original = "local x = 1";
    std::vector<TextEdit> edits = {
        {{{0, 6}, {0, 7}}, "y"}  // Replace "x" with "y"
    };

    auto result = SourceMapping::fromEdits(original, edits);

    CHECK(result.transformedSource == "local y = 1");
    CHECK(!result.edits.empty());

    SourceMapping mapping{std::move(result.edits)};

    // Position before edit unchanged
    auto pos = mapping.originalToTransformed({0, 0});
    CHECK(pos.has_value());
    CHECK(pos->line == 0);
    CHECK(pos->column == 0);

    // Position after edit unchanged (same length replacement)
    pos = mapping.originalToTransformed({0, 10});
    CHECK(pos.has_value());
    CHECK(pos->line == 0);
    CHECK(pos->column == 10);
}

TEST_CASE("SourceMapping.fromEdits single replacement longer")
{
    std::string original = "local x = require(file)";
    std::vector<TextEdit> edits = {
        {{{0, 18}, {0, 22}}, "\"path/to/file\""}  // Replace "file" with "path/to/file"
    };

    auto result = SourceMapping::fromEdits(original, edits);

    CHECK(result.transformedSource == "local x = require(\"path/to/file\")");

    SourceMapping mapping{std::move(result.edits)};

    // Position before edit unchanged
    auto pos = mapping.originalToTransformed({0, 10});
    CHECK(pos.has_value());
    CHECK(pos->line == 0);
    CHECK(pos->column == 10);

    // Position after edit shifted
    pos = mapping.originalToTransformed({0, 23});  // After closing paren in original
    CHECK(pos.has_value());
    CHECK(pos->line == 0);
    CHECK(pos->column == 33);  // Shifted by 10 chars
}

TEST_CASE("SourceMapping.fromEdits single replacement shorter")
{
    std::string original = "local longVariableName = 1";
    std::vector<TextEdit> edits = {
        {{{0, 6}, {0, 22}}, "x"}  // Replace "longVariableName" with "x"
    };

    auto result = SourceMapping::fromEdits(original, edits);

    CHECK(result.transformedSource == "local x = 1");

    SourceMapping mapping{std::move(result.edits)};

    // Position after edit shifted back
    auto pos = mapping.originalToTransformed({0, 25});
    CHECK(pos.has_value());
    CHECK(pos->line == 0);
    CHECK(pos->column == 10);
}

TEST_CASE("SourceMapping.fromEdits multiple edits")
{
    std::string original = "a b c";
    std::vector<TextEdit> edits = {
        {{{0, 0}, {0, 1}}, "AAA"},  // a -> AAA
        {{{0, 4}, {0, 5}}, "CCC"},  // c -> CCC
    };

    auto result = SourceMapping::fromEdits(original, edits);
    CHECK(result.transformedSource == "AAA b CCC");
}

TEST_CASE("SourceMapping.fromEdits three edits same line")
{
    // Test cumulative column delta with 3+ edits on same line
    std::string original = "a b c d";
    std::vector<TextEdit> edits = {
        {{{0, 0}, {0, 1}}, "AAA"},  // a -> AAA (+2 chars)
        {{{0, 2}, {0, 3}}, "BBB"},  // b -> BBB (+2 chars)
        {{{0, 4}, {0, 5}}, "CCC"},  // c -> CCC (+2 chars)
    };

    auto result = SourceMapping::fromEdits(original, edits);
    CHECK(result.transformedSource == "AAA BBB CCC d");

    SourceMapping mapping{std::move(result.edits)};

    // Test position mapping after all three edits
    // Position of 'd' in original is column 6
    // After edits: +2 (first) +2 (second) +2 (third) = +6
    // So 'd' should be at column 12 in transformed
    auto pos = mapping.originalToTransformed({0, 6});
    CHECK(pos.has_value());
    CHECK(pos->line == 0);
    CHECK(pos->column == 12);

    // Reverse mapping
    auto origPos = mapping.transformedToOriginal({0, 12});
    CHECK(origPos.has_value());
    CHECK(origPos->line == 0);
    CHECK(origPos->column == 6);
}

TEST_CASE("SourceMapping.fromEdits three edits same line shrinking")
{
    // Test with edits that shrink the text
    std::string original = "aaa bbb ccc ddd";
    std::vector<TextEdit> edits = {
        {{{0, 0}, {0, 3}}, "a"},    // aaa -> a (-2 chars)
        {{{0, 4}, {0, 7}}, "b"},    // bbb -> b (-2 chars)
        {{{0, 8}, {0, 11}}, "c"},   // ccc -> c (-2 chars)
    };

    auto result = SourceMapping::fromEdits(original, edits);
    CHECK(result.transformedSource == "a b c ddd");

    SourceMapping mapping{std::move(result.edits)};

    // Position of 'ddd' in original is column 12
    // After edits: -2 (first) -2 (second) -2 (third) = -6
    // So 'ddd' should be at column 6 in transformed
    auto pos = mapping.originalToTransformed({0, 12});
    CHECK(pos.has_value());
    CHECK(pos->line == 0);
    CHECK(pos->column == 6);
}

TEST_CASE("SourceMapping.fromEdits adjacent edits same line")
{
    // Test edits that are adjacent (no gap between them)
    std::string original = "abcd";
    std::vector<TextEdit> edits = {
        {{{0, 0}, {0, 1}}, "XX"},   // a -> XX
        {{{0, 1}, {0, 2}}, "YY"},   // b -> YY
        {{{0, 2}, {0, 3}}, "ZZ"},   // c -> ZZ
    };

    auto result = SourceMapping::fromEdits(original, edits);
    CHECK(result.transformedSource == "XXYYZZd");

    SourceMapping mapping{std::move(result.edits)};

    // 'd' was at column 3, now should be at column 6
    auto pos = mapping.originalToTransformed({0, 3});
    CHECK(pos.has_value());
    CHECK(pos->line == 0);
    CHECK(pos->column == 6);
}

TEST_CASE("SourceMapping.fromEdits mixed size edits same line")
{
    // Test with mix of growing, shrinking, and same-size edits
    std::string original = "aa bb cc dd";
    std::vector<TextEdit> edits = {
        {{{0, 0}, {0, 2}}, "AAAA"},   // aa -> AAAA (+2)
        {{{0, 3}, {0, 5}}, "B"},      // bb -> B (-1)
        {{{0, 6}, {0, 8}}, "CC"},     // cc -> CC (same)
        {{{0, 9}, {0, 11}}, "DDD"},   // dd -> DDD (+1)
    };

    auto result = SourceMapping::fromEdits(original, edits);
    CHECK(result.transformedSource == "AAAA B CC DDD");

    SourceMapping mapping{std::move(result.edits)};

    // Position inside third edit range (col 6 = start of 'cc') maps to transformed start of that edit
    // Transformed: "AAAA B CC DDD" — 'CC' starts at col 7
    auto pos = mapping.originalToTransformed({0, 6});
    CHECK(pos.has_value());
    CHECK(pos->line == 0);
    CHECK(pos->column == 7);

    // Space between first and second edit (col 2 in original)
    // After first edit grows by 2: col 2 + 2 = col 4
    pos = mapping.originalToTransformed({0, 2});
    CHECK(pos.has_value());
    CHECK(pos->line == 0);
    CHECK(pos->column == 4);
}

TEST_CASE("SourceMapping.fromEdits position mapping between same-line edits")
{
    std::string original = "a   b   c";
    //                      0123456789
    std::vector<TextEdit> edits = {
        {{{0, 0}, {0, 1}}, "XX"},   // a -> XX at col 0-1, (+1)
        {{{0, 4}, {0, 5}}, "YYY"},  // b -> YYY at col 4-5, (+2)
    };

    auto result = SourceMapping::fromEdits(original, edits);
    CHECK(result.transformedSource == "XX   YYY   c");

    SourceMapping mapping{std::move(result.edits)};

    // Position in gap between first and second edit (e.g., column 2 = first space)
    // After first edit: +1, so column 2 -> column 3
    auto pos = mapping.originalToTransformed({0, 2});
    CHECK(pos.has_value());
    CHECK(pos->line == 0);
    CHECK(pos->column == 3);

    // Position after second edit (column 8 = 'c')
    // After first edit: +1, after second edit: +2, total = +3
    pos = mapping.originalToTransformed({0, 8});
    CHECK(pos.has_value());
    CHECK(pos->line == 0);
    CHECK(pos->column == 11);

    // Reverse mapping for position after edits
    auto origPos = mapping.transformedToOriginal({0, 11});
    CHECK(origPos.has_value());
    CHECK(origPos->line == 0);
    CHECK(origPos->column == 8);
}

TEST_CASE("SourceMapping.fromEdits multiline")
{
    std::string original = "line1\nline2\nline3";
    std::vector<TextEdit> edits = {
        {{{1, 0}, {1, 5}}, "replaced"}
    };

    auto result = SourceMapping::fromEdits(original, edits);
    CHECK(result.transformedSource == "line1\nreplaced\nline3");
}

TEST_CASE("SourceMapping.fromEdits insert (empty range)")
{
    std::string original = "local x = 1";
    std::vector<TextEdit> edits = {
        {{{0, 0}, {0, 0}}, "-- comment\n"}  // Insert at start
    };

    auto result = SourceMapping::fromEdits(original, edits);
    CHECK(result.transformedSource == "-- comment\nlocal x = 1");

    SourceMapping mapping{std::move(result.edits)};

    // Position after insert is shifted
    auto pos = mapping.originalToTransformed({0, 0});
    CHECK(pos.has_value());
    CHECK(pos->line == 1);  // Now on line 1
    CHECK(pos->column == 0);
}

TEST_CASE("SourceMapping.fromEdits delete (empty newText)")
{
    std::string original = "local x = 1 -- comment";
    std::vector<TextEdit> edits = {
        {{{0, 11}, {0, 22}}, ""}  // Delete " -- comment"
    };

    auto result = SourceMapping::fromEdits(original, edits);
    CHECK(result.transformedSource == "local x = 1");
}

TEST_CASE("SourceMapping.transformedToOriginal basic")
{
    std::string original = "local x = require(file)";
    std::vector<TextEdit> edits = {
        {{{0, 18}, {0, 22}}, "\"path/to/file\""}
    };

    auto result = SourceMapping::fromEdits(original, edits);
    SourceMapping mapping{std::move(result.edits)};

    // Position before edit
    auto pos = mapping.transformedToOriginal({0, 10});
    CHECK(pos.has_value());
    CHECK(pos->line == 0);
    CHECK(pos->column == 10);

    // Position after edit (in transformed) maps back correctly
    pos = mapping.transformedToOriginal({0, 33});
    CHECK(pos.has_value());
    CHECK(pos->line == 0);
    CHECK(pos->column == 23);
}

TEST_CASE("SourceMapping.fromEdits rejects overlapping edits")
{
    std::string original = "local x = 1";
    std::vector<TextEdit> edits = {
        {{{0, 0}, {0, 5}}, "const"},
        {{{0, 3}, {0, 7}}, "y"},  // Overlaps with first edit
    };

    CHECK_THROWS_AS(SourceMapping::fromEdits(original, edits), std::runtime_error);
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("PluginTextDocument");

TEST_CASE("PluginTextDocument getText returns transformed")
{
    std::string original = "local x = 1";
    std::string transformed = "local y = 1";
    std::vector<TextEdit> edits = {
        {{{0, 6}, {0, 7}}, "y"}
    };

    auto result = SourceMapping::fromEdits(original, edits);
    PluginTextDocument doc(Uri::parse("file://test.luau"), "luau", 1, original, transformed, SourceMapping{std::move(result.edits)});

    CHECK(doc.getText() == transformed);
    CHECK(doc.getOriginalText() == original);
}

TEST_CASE("PluginTextDocument convertPosition maps correctly")
{
    std::string original = "local x = require(file)";
    std::vector<TextEdit> edits = {
        {{{0, 18}, {0, 22}}, "\"path/to/file\""}
    };

    auto result = SourceMapping::fromEdits(original, edits);
    PluginTextDocument doc(
        Uri::parse("file://test.luau"),
        "luau",
        1,
        original,
        std::move(result.transformedSource),
        SourceMapping{std::move(result.edits)}
    );

    // LSP position -> Luau position (original -> transformed)
    lsp::Position lspPos{0, 10};  // Position in original
    auto luauPos = doc.convertPosition(lspPos);
    CHECK(luauPos.line == 0);
    CHECK(luauPos.column == 10);  // Before edit, unchanged

    // Luau position -> LSP position (transformed -> original)
    Luau::Position transformedPos{0, 33};  // After edit in transformed
    auto backLspPos = doc.convertPosition(transformedPos);
    CHECK(backLspPos.line == 0);
    CHECK(backLspPos.character == 23);  // Maps back to original
}

TEST_CASE("PluginTextDocument convertPosition with multiline insertion")
{
    // Plugin inserts a longer line before short original content.
    // The bug: base TextDocument::convertPosition uses getLineOffsets() (virtual,
    // returns transformed offsets) but substr's from _content (original).
    // When lines have different lengths, the offset mismatch corrupts columns.
    std::string original = "ab\ncd";
    // Insert a longer header line so the transformed line offsets diverge from original
    std::vector<TextEdit> edits = {
        {{{0, 0}, {0, 0}}, "-- long header line\n"}
    };

    auto result = SourceMapping::fromEdits(original, edits);
    // transformed = "-- long header line\nab\ncd"
    // original offsets:    [0, 3]         (line 0: "ab\n", line 1: "cd")
    // transformed offsets: [0, 20, 23]    (line 0: "-- long header line\n", line 1: "ab\n", line 2: "cd")
    PluginTextDocument doc(
        Uri::parse("file://test.luau"),
        "luau",
        1,
        original,
        std::move(result.transformedSource),
        SourceMapping{std::move(result.edits)}
    );

    // LSP position {1, 1} is column 1 on line 1 of ORIGINAL ("cd")
    // Should map to {2, 1} in transformed
    lsp::Position lspPos{1, 1};
    auto luauPos = doc.convertPosition(lspPos);
    CHECK(luauPos.line == 2);
    CHECK(luauPos.column == 1);
}

TEST_CASE("PluginTextDocument convertPosition reverse with multiline insertion")
{
    // Reverse: Luau position in transformed -> LSP position in original.
    // Same mismatch: base convertPosition(Luau::Position) uses getLineOffsets()
    // (transformed) with _content (original).
    std::string original = "ab\ncd";
    std::vector<TextEdit> edits = {
        {{{0, 0}, {0, 0}}, "-- long header line\n"}
    };

    auto result = SourceMapping::fromEdits(original, edits);
    PluginTextDocument doc(
        Uri::parse("file://test.luau"),
        "luau",
        1,
        original,
        std::move(result.transformedSource),
        SourceMapping{std::move(result.edits)}
    );

    // Luau position {2, 1} in transformed is "d" on line 2 ("cd")
    // Should map back to LSP position {1, 1} in original
    Luau::Position transformedPos{2, 1};
    auto lspPos = doc.convertPosition(transformedPos);
    CHECK(lspPos.line == 1);
    CHECK(lspPos.character == 1);
}

TEST_CASE("PluginTextDocument convertPosition with line-changing edit preserves round trip")
{
    // A more complex case: plugin wraps code, adding lines both before and after.
    // Round-tripping original->transformed->original should give back the same position.
    std::string original = "print('hello')";
    std::vector<TextEdit> edits = {
        {{{0, 0}, {0, 0}}, "do\n"},           // Insert "do\n" at start
        {{{0, 14}, {0, 14}}, "\nend"}          // Append "\nend" at end
    };

    auto result = SourceMapping::fromEdits(original, edits);
    // transformed = "do\nprint('hello')\nend"
    PluginTextDocument doc(
        Uri::parse("file://test.luau"),
        "luau",
        1,
        original,
        std::move(result.transformedSource),
        SourceMapping{std::move(result.edits)}
    );

    // Original position {0, 6} -> should map to transformed {1, 6} -> back to {0, 6}
    lsp::Position originalPos{0, 6};
    auto luauPos = doc.convertPosition(originalPos);
    auto roundTripped = doc.convertPosition(luauPos);
    CHECK(roundTripped.line == originalPos.line);
    CHECK(roundTripped.character == originalPos.character);
}

TEST_CASE("PluginTextDocument lineCount uses transformed")
{
    std::string original = "local x = 1";
    std::vector<TextEdit> edits = {
        {{{0, 0}, {0, 0}}, "-- header\n"}  // Insert line at start
    };

    auto result = SourceMapping::fromEdits(original, edits);
    PluginTextDocument doc(
        Uri::parse("file://test.luau"),
        "luau",
        1,
        original,
        std::move(result.transformedSource),
        SourceMapping{std::move(result.edits)}
    );

    CHECK(doc.lineCount() == 2);  // Transformed has 2 lines
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("PluginRuntime");

TEST_CASE_FIXTURE(Fixture, "PluginRuntime loads valid plugin")
{

    std::string pluginPath = tempDir.write_child("valid_plugin.luau", R"(
return {
    transformSource = function(source, context)
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    auto error = runtime.load();

    CHECK(!error.has_value());
    CHECK(runtime.isLoaded());
}

TEST_CASE_FIXTURE(Fixture, "PluginRuntime fails on non-existent file")
{
    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file("/non/existent/path/plugin.luau"));
    auto error = runtime.load();

    CHECK(error.has_value());
    CHECK(error->message.find("Failed to open") != std::string::npos);
    CHECK(!runtime.isLoaded());
}

TEST_CASE_FIXTURE(Fixture, "PluginRuntime fails on syntax error")
{

    std::string pluginPath = tempDir.write_child("syntax_error.luau", R"(
return {
    this is not valid lua syntax!!!
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    auto error = runtime.load();

    CHECK(error.has_value());
    CHECK(!runtime.isLoaded());
}

TEST_CASE_FIXTURE(Fixture, "PluginRuntime fails when plugin does not return table")
{

    std::string pluginPath = tempDir.write_child("no_table.luau", R"(
return "not a table"
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    auto error = runtime.load();

    CHECK(error.has_value());
    CHECK(error->message.find("must return a table") != std::string::npos);
    CHECK(!runtime.isLoaded());
}

TEST_CASE_FIXTURE(Fixture, "PluginRuntime fails when plugin returns nil")
{

    std::string pluginPath = tempDir.write_child("returns_nil.luau", R"(
return nil
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    auto error = runtime.load();

    CHECK(error.has_value());
    CHECK(error->message.find("must return a table") != std::string::npos);
    CHECK(!runtime.isLoaded());
}

TEST_CASE_FIXTURE(Fixture, "PluginRuntime loads plugin without transformSource (optional)")
{

    std::string pluginPath = tempDir.write_child("no_transform.luau", R"(
return {
    someOtherFunction = function() end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    auto error = runtime.load();

    CHECK(!error.has_value());
    CHECK(runtime.isLoaded());
    CHECK(!runtime.hasTransformSource());

    // Calling transformSource should return empty edits
    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);
    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
    auto& edits = std::get<std::vector<TextEdit>>(result);
    CHECK(edits.empty());
}

TEST_CASE_FIXTURE(Fixture, "PluginRuntime fails when transformSource is not a function")
{

    std::string pluginPath = tempDir.write_child("not_function.luau", R"(
return {
    transformSource = "not a function"
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    auto error = runtime.load();

    CHECK(error.has_value());
    CHECK(error->message.find("transformSource") != std::string::npos);
    CHECK(!runtime.isLoaded());
}

TEST_CASE_FIXTURE(Fixture, "PluginRuntime transformSource returns nil for no changes")
{

    std::string pluginPath = tempDir.write_child("no_changes.luau", R"(
return {
    transformSource = function(source, context)
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
    auto& edits = std::get<std::vector<TextEdit>>(result);
    CHECK(edits.empty());
}

TEST_CASE_FIXTURE(Fixture, "PluginRuntime transformSource returns empty table for no changes")
{

    std::string pluginPath = tempDir.write_child("empty_table.luau", R"(
return {
    transformSource = function(source, context)
        return {}
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
    auto& edits = std::get<std::vector<TextEdit>>(result);
    CHECK(edits.empty());
}

TEST_CASE_FIXTURE(Fixture, "PluginRuntime transformSource returns edits")
{

    std::string pluginPath = tempDir.write_child("with_edits.luau", R"(
return {
    transformSource = function(source, context)
        return {
            {
                startLine = 1, startColumn = 7,
                endLine = 1, endColumn = 8,
                newText = "y"
            }
        }
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
    auto& edits = std::get<std::vector<TextEdit>>(result);
    REQUIRE(edits.size() == 1);
    CHECK(edits[0].range.begin.line == 0);
    CHECK(edits[0].range.begin.column == 6);
    CHECK(edits[0].range.end.line == 0);
    CHECK(edits[0].range.end.column == 7);
    CHECK(edits[0].newText == "y");
}

TEST_CASE_FIXTURE(Fixture, "PluginRuntime transformSource receives context")
{

    std::string pluginPath = tempDir.write_child("check_context.luau", R"(
return {
    transformSource = function(source, context)
        -- Verify we receive the context
        assert(context.filePath == "test/file.luau", "filePath mismatch")
        assert(context.moduleName == "TestModule", "moduleName mismatch")
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test/file.luau", "TestModule"};
    auto result = runtime.transformSource("local x = 1", ctx);

    // If context was wrong, we'd get an error from the assert
    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_CASE_FIXTURE(Fixture, "PluginRuntime transformSource handles runtime error")
{

    std::string pluginPath = tempDir.write_child("runtime_error.luau", R"(
return {
    transformSource = function(source, context)
        error("Something went wrong!")
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<PluginError>(result));
    auto& error = std::get<PluginError>(result);
    CHECK(error.message.find("Something went wrong") != std::string::npos);
}

TEST_CASE_FIXTURE(Fixture, "PluginRuntime transformSource handles invalid edit structure")
{

    std::string pluginPath = tempDir.write_child("invalid_edit.luau", R"(
return {
    transformSource = function(source, context)
        return {
            { notAnEdit = true }
        }
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<PluginError>(result));
}

TEST_CASE_FIXTURE(Fixture, "PluginRuntime load times out on infinite loop")
{

    std::string pluginPath = tempDir.write_child("infinite_loop.luau", R"(
-- This plugin has an infinite loop during load
while true do
end

return {
    transformSource = function(source, context)
        return nil
    end
}
)");

    // Use a very short timeout (10ms)
    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath), 10);
    auto error = runtime.load();

    CHECK(error.has_value());
    CHECK(error->message.find("timed out") != std::string::npos);
    CHECK(!runtime.isLoaded());
}

TEST_CASE_FIXTURE(Fixture, "PluginRuntime transformSource times out on infinite loop")
{

    std::string pluginPath = tempDir.write_child("infinite_transform.luau", R"(
return {
    transformSource = function(source, context)
        -- Infinite loop during transform
        while true do
        end
        return nil
    end
}
)");

    // Use a very short timeout (10ms)
    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath), 10);
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<PluginError>(result));
    auto& error = std::get<PluginError>(result);
    CHECK(error.message.find("timed out") != std::string::npos);
}

TEST_CASE_FIXTURE(Fixture, "PluginRuntime with long timeout completes successfully")
{

    std::string pluginPath = tempDir.write_child("valid_plugin.luau", R"(
return {
    transformSource = function(source, context)
        -- Do some work but not an infinite loop
        local sum = 0
        for i = 1, 1000 do
            sum = sum + i
        end
        return nil
    end
}
)");

    // Use a longer timeout (5 seconds)
    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath), 5000);
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("PluginManager");

TEST_CASE_FIXTURE(Fixture, "PluginManager configure loads plugins")
{

    std::string plugin1 = tempDir.write_child("plugin1.luau", R"(
return {
    transformSource = function(source, context)
        return nil
    end
}
)");
    std::string plugin2 = tempDir.write_child("plugin2.luau", R"(
return {
    transformSource = function(source, context)
        return nil
    end
}
)");

    PluginManager manager(client.get(), getWorkspaceNotNull(*this));
    size_t loaded = manager.configure({plugin1, plugin2});

    CHECK(loaded == 2);
    CHECK(manager.pluginCount() == 2);
    CHECK(manager.hasPlugins());
}

TEST_CASE_FIXTURE(Fixture, "PluginManager configure handles invalid plugins gracefully")
{

    std::string validPlugin = tempDir.write_child("valid.luau", R"(
return {
    transformSource = function(source, context)
        return nil
    end
}
)");
    std::string invalidPlugin = tempDir.write_child("invalid.luau", R"(
return "not a table"
)");

    PluginManager manager(client.get(), getWorkspaceNotNull(*this));
    size_t loaded = manager.configure({validPlugin, invalidPlugin});

    CHECK(loaded == 1);
    CHECK(manager.pluginCount() == 1);

    // Verify error was logged for invalid plugin
    auto errors = getLogMessages(*client, lsp::MessageType::Error);
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("invalid.luau") != std::string::npos);
    CHECK(errors[0].find("must return a table") != std::string::npos);
}

TEST_CASE_FIXTURE(Fixture, "PluginManager transform with no plugins returns empty")
{
    PluginManager manager(client.get(), getWorkspaceNotNull(*this));
    auto edits = manager.transform("local x = 1", Uri::parse("file:///test.luau"), "test");

    CHECK(edits.empty());
}

TEST_CASE_FIXTURE(Fixture, "PluginManager transform applies plugin edits")
{

    std::string pluginPath = tempDir.write_child("transform.luau", R"(
return {
    transformSource = function(source, context)
        return {
            {
                startLine = 1, startColumn = 7,
                endLine = 1, endColumn = 8,
                newText = "y"
            }
        }
    end
}
)");

    PluginManager manager(client.get(), getWorkspaceNotNull(*this));
    manager.configure({pluginPath});

    auto edits = manager.transform("local x = 1", Uri::parse("file:///test.luau"), "test");

    REQUIRE(edits.size() == 1);
    CHECK(edits[0].newText == "y");
}

TEST_CASE_FIXTURE(Fixture, "PluginManager clear removes all plugins")
{

    std::string pluginPath = tempDir.write_child("plugin.luau", R"(
return {
    transformSource = function(source, context)
        return nil
    end
}
)");

    PluginManager manager(client.get(), getWorkspaceNotNull(*this));
    manager.configure({pluginPath});
    CHECK(manager.hasPlugins());

    manager.clear();
    CHECK(!manager.hasPlugins());
    CHECK(manager.pluginCount() == 0);
}

TEST_CASE_FIXTURE(Fixture, "PluginManager SourceMapping builds correct mapping from edits")
{
    std::string source = "local x = 1";
    std::vector<TextEdit> edits = {
        {{{0, 6}, {0, 7}}, "y"}
    };

    auto result = SourceMapping::fromEdits(source, edits);

    CHECK(result.transformedSource == "local y = 1");
    CHECK(!result.edits.empty());
}

TEST_CASE_FIXTURE(Fixture, "PluginManager combines edits from multiple plugins")
{


    // Plugin 1: replace "x" with "foo"
    std::string plugin1Path = tempDir.write_child("plugin1.luau", R"(
return {
    transformSource = function(source, context)
        return {
            {
                startLine = 1, startColumn = 7,
                endLine = 1, endColumn = 8,
                newText = "foo"
            }
        }
    end
}
)");

    // Plugin 2: replace "1" with "42"
    std::string plugin2Path = tempDir.write_child("plugin2.luau", R"(
return {
    transformSource = function(source, context)
        return {
            {
                startLine = 1, startColumn = 11,
                endLine = 1, endColumn = 12,
                newText = "42"
            }
        }
    end
}
)");

    PluginManager manager(client.get(), getWorkspaceNotNull(*this));
    manager.configure({plugin1Path, plugin2Path});

    auto edits = manager.transform("local x = 1", Uri::parse("file:///test.luau"), "test");

    // Both plugins' edits should be combined
    REQUIRE(edits.size() == 2);
}

TEST_CASE_FIXTURE(Fixture, "PluginManager overlapping edits rejected by SourceMapping")
{


    // Plugin 1: replace columns 6-10
    std::string plugin1Path = tempDir.write_child("plugin1.luau", R"(
return {
    transformSource = function(source, context)
        return {
            {
                startLine = 1, startColumn = 7,
                endLine = 1, endColumn = 11,
                newText = "foo"
            }
        }
    end
}
)");

    // Plugin 2: edits overlapping range (columns 8-11)
    std::string plugin2Path = tempDir.write_child("plugin2.luau", R"(
return {
    transformSource = function(source, context)
        return {
            {
                startLine = 1, startColumn = 9,
                endLine = 1, endColumn = 12,
                newText = "bar"
            }
        }
    end
}
)");

    PluginManager manager(client.get(), getWorkspaceNotNull(*this));
    manager.configure({plugin1Path, plugin2Path});

    auto edits = manager.transform("local x = 1", Uri::parse("file:///test.luau"), "test");

    // transform() returns the raw edits; overlap is caught when building the SourceMapping
    CHECK(!edits.empty());
    CHECK_THROWS_AS(SourceMapping::fromEdits("local x = 1", edits), std::runtime_error);
}

TEST_CASE_FIXTURE(Fixture, "PluginManager reload reloads plugins from disk")
{

    std::string pluginPath = tempDir.write_child("plugin.luau", R"(
return {
    transformSource = function(source, context)
        return {
            {
                startLine = 1, startColumn = 7,
                endLine = 1, endColumn = 8,
                newText = "y"
            }
        }
    end
}
)");

    PluginManager manager(client.get(), getWorkspaceNotNull(*this));
    manager.configure({pluginPath});

    // Verify initial transformation: "x" -> "y"
    auto edits = manager.transform("local x = 1", Uri::parse("file:///test.luau"), "test");
    REQUIRE(edits.size() == 1);
    CHECK(edits[0].newText == "y");

    // Update the plugin file on disk to replace with "z" instead
    tempDir.write_child("plugin.luau", R"(
return {
    transformSource = function(source, context)
        return {
            {
                startLine = 1, startColumn = 7,
                endLine = 1, endColumn = 8,
                newText = "z"
            }
        }
    end
}
)");

    // Reload and verify updated transformation
    size_t reloaded = manager.reload();
    CHECK(reloaded == 1);

    edits = manager.transform("local x = 1", Uri::parse("file:///test.luau"), "test");
    REQUIRE(edits.size() == 1);
    CHECK(edits[0].newText == "z");
}

TEST_CASE_FIXTURE(Fixture, "PluginManager reload handles plugin that becomes invalid")
{

    std::string pluginPath = tempDir.write_child("plugin.luau", R"(
return {
    transformSource = function(source, context)
        return nil
    end
}
)");

    PluginManager manager(client.get(), getWorkspaceNotNull(*this));
    manager.configure({pluginPath});
    CHECK(manager.pluginCount() == 1);

    // Overwrite plugin with invalid content
    tempDir.write_child("plugin.luau", R"(
return "not a table"
)");

    size_t reloaded = manager.reload();
    CHECK(reloaded == 0);

    // Plugin count remains the same (still in the list, just failed to load)
    CHECK(manager.pluginCount() == 1);

    auto errors = getLogMessages(*client, lsp::MessageType::Error);
    CHECK(!errors.empty());
}

TEST_CASE_FIXTURE(Fixture, "PluginManager isPluginFile matches loaded plugins")
{

    std::string pluginPath = tempDir.write_child("plugin.luau", R"(
return {}
)");

    PluginManager manager(client.get(), getWorkspaceNotNull(*this));
    manager.configure({pluginPath});

    CHECK(manager.isPluginFile(Uri::file(pluginPath)));
    CHECK_FALSE(manager.isPluginFile(Uri::parse("file:///some/other/file.luau")));
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("PluginFilesystemAPI");

TEST_CASE_FIXTURE(Fixture, "lsp.workspace.getRootUri returns Uri userdata")
{

    std::string pluginPath = tempDir.write_child("test_rooturi.luau", R"(
return {
    transformSource = function(source, context)
        local rootUri = lsp.workspace.getRootUri()
        assert(rootUri ~= nil, "getRootUri returned nil")
        assert(rootUri.scheme == "file", "scheme mismatch: " .. tostring(rootUri.scheme))
        assert(rootUri.fsPath ~= nil, "fsPath is nil")
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_CASE_FIXTURE(Fixture, "lsp.Uri.parse creates Uri userdata")
{

    std::string pluginPath = tempDir.write_child("test_uri_parse.luau", R"(
return {
    transformSource = function(source, context)
        local uri = lsp.Uri.parse("file:///test/path/file.luau")
        assert(uri ~= nil, "Uri.parse returned nil")
        assert(uri.scheme == "file", "scheme mismatch: " .. tostring(uri.scheme))
        assert(uri.path == "/test/path/file.luau", "path mismatch: " .. tostring(uri.path))
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_CASE_FIXTURE(Fixture, "lsp.Uri.file creates file Uri")
{

    std::string pluginPath = tempDir.write_child("test_uri_file.luau", R"(
return {
    transformSource = function(source, context)
        local uri = lsp.Uri.file("/test/path/file.luau")
        assert(uri ~= nil, "Uri.file returned nil")
        assert(uri.scheme == "file", "scheme mismatch: " .. tostring(uri.scheme))
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_CASE_FIXTURE(Fixture, "Uri:joinPath works correctly")
{

    std::string pluginPath = tempDir.write_child("test_joinpath.luau", R"(
return {
    transformSource = function(source, context)
        local rootUri = lsp.workspace.getRootUri()
        local newUri = rootUri:joinPath("subdir", "file.luau")
        assert(newUri ~= nil, "joinPath returned nil")
        assert(newUri.scheme == "file", "scheme mismatch")
        -- Path should contain the joined segments
        local pathStr = newUri:toString()
        assert(string.find(pathStr, "subdir") ~= nil, "subdir not in path")
        assert(string.find(pathStr, "file.luau") ~= nil, "file.luau not in path")
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_CASE_FIXTURE(Fixture, "Uri:toString returns string")
{

    std::string pluginPath = tempDir.write_child("test_tostring.luau", R"(
return {
    transformSource = function(source, context)
        local uri = lsp.Uri.parse("file:///test/path/file.luau")
        local str = uri:toString()
        assert(type(str) == "string", "toString did not return string")
        assert(string.find(str, "file://") ~= nil, "toString result missing scheme")
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_CASE_FIXTURE(Fixture, "lsp.fs.readFile throws when disabled")
{

    // Filesystem access is disabled by default

    std::string testFilePath = tempDir.write_child("testdata.txt", "Hello, World!");

    std::string pluginPath = tempDir.write_child("test_readfile_disabled.luau", R"(
return {
    transformSource = function(source, context)
        local rootUri = lsp.workspace.getRootUri()
        local fileUri = rootUri:joinPath("testdata.txt")
        local ok, err = pcall(function()
            return lsp.fs.readFile(fileUri)
        end)
        assert(not ok, "readFile should have thrown")
        assert(string.find(err, "filesystem access not available") ~= nil, "wrong error: " .. tostring(err))
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_CASE_FIXTURE(Fixture, "lsp.fs.readFile throws for non-file URIs")
{

    // Enable filesystem access for this test
    client->globalConfig.plugins.fileSystem.enabled = true;

    std::string pluginPath = tempDir.write_child("test_readfile_https.luau", R"(
return {
    transformSource = function(source, context)
        local httpsUri = lsp.Uri.parse("https://example.com/file.txt")
        local ok, err = pcall(function()
            return lsp.fs.readFile(httpsUri)
        end)
        assert(not ok, "readFile should have thrown")
        assert(string.find(err, "only file://") ~= nil, "wrong error: " .. tostring(err))
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_CASE_FIXTURE(Fixture, "Uri equality works")
{

    std::string pluginPath = tempDir.write_child("test_uri_equality.luau", R"(
return {
    transformSource = function(source, context)
        local uri1 = lsp.Uri.parse("file:///test/path/file.luau")
        local uri2 = lsp.Uri.parse("file:///test/path/file.luau")
        local uri3 = lsp.Uri.parse("file:///other/path/file.luau")

        assert(uri1 == uri2, "equal URIs should be equal")
        assert(uri1 ~= uri3, "different URIs should not be equal")
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_CASE_FIXTURE(Fixture, "lsp.fs.listDirectory lists directory entries")
{
    // Enable filesystem access
    client->globalConfig.plugins.fileSystem.enabled = true;

    // Create some files in a subdirectory within the workspace temp dir
    tempDir.write_child("subdir/file1.luau", "");
    tempDir.write_child("subdir/file2.luau", "");

    std::string pluginPath = tempDir.write_child("test_listdir.luau", R"(
return {
    transformSource = function(source, context)
        local rootUri = lsp.workspace.getRootUri()
        local dirUri = rootUri:joinPath("subdir")
        local entries = lsp.fs.listDirectory(dirUri)
        assert(type(entries) == "table", "listDirectory should return a table")
        assert(#entries >= 2, "expected at least 2 entries, got " .. #entries)
        -- Each entry should be a Uri
        for _, entry in entries do
            assert(entry.scheme == "file", "entry should have file scheme")
            assert(entry.fsPath ~= nil, "entry should have fsPath")
        end
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_CASE_FIXTURE(Fixture, "lsp.fs.listDirectory throws when disabled")
{
    // Filesystem access disabled by default

    std::string pluginPath = tempDir.write_child("test_listdir_disabled.luau", R"(
return {
    transformSource = function(source, context)
        local rootUri = lsp.workspace.getRootUri()
        local ok, err = pcall(function()
            return lsp.fs.listDirectory(rootUri)
        end)
        assert(not ok, "listDirectory should have thrown")
        assert(string.find(err, "filesystem access not available") ~= nil, "wrong error: " .. tostring(err))
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_CASE_FIXTURE(Fixture, "lsp.fs.listDirectory throws for non-directory")
{
    client->globalConfig.plugins.fileSystem.enabled = true;

    tempDir.write_child("afile.txt", "content");

    std::string pluginPath = tempDir.write_child("test_listdir_file.luau", R"(
return {
    transformSource = function(source, context)
        local rootUri = lsp.workspace.getRootUri()
        local fileUri = rootUri:joinPath("afile.txt")
        local ok, err = pcall(function()
            return lsp.fs.listDirectory(fileUri)
        end)
        assert(not ok, "listDirectory should have thrown for a file")
        assert(string.find(err, "not a directory") ~= nil, "wrong error: " .. tostring(err))
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("PluginLoggingAPI");

TEST_CASE_FIXTURE(Fixture, "lsp.client.sendLogMessage sends message with prefix")
{

    std::string pluginPath = tempDir.write_child("test_log.luau", R"(
return {
    transformSource = function(source, context)
        lsp.client.sendLogMessage("info", "Test message")
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));

    // Verify log message was sent with prefix
    auto infoMessages = getLogMessages(*client, lsp::MessageType::Info);
    REQUIRE(infoMessages.size() >= 1);
    bool foundMessage = false;
    for (const auto& msg : infoMessages)
    {
        if (msg.find("[Plugin") != std::string::npos && msg.find("Test message") != std::string::npos)
        {
            foundMessage = true;
            break;
        }
    }
    CHECK(foundMessage);
}

TEST_CASE_FIXTURE(Fixture, "lsp.client.sendLogMessage supports all message types")
{

    std::string pluginPath = tempDir.write_child("test_log_types.luau", R"(
return {
    transformSource = function(source, context)
        lsp.client.sendLogMessage("error", "Error msg")
        lsp.client.sendLogMessage("warning", "Warning msg")
        lsp.client.sendLogMessage("info", "Info msg")
        lsp.client.sendLogMessage("log", "Log msg")
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));

    // Verify all message types were sent
    auto errorMsgs = getLogMessages(*client, lsp::MessageType::Error);
    auto warningMsgs = getLogMessages(*client, lsp::MessageType::Warning);
    auto infoMsgs = getLogMessages(*client, lsp::MessageType::Info);
    auto logMsgs = getLogMessages(*client, lsp::MessageType::Log);

    CHECK(!errorMsgs.empty());
    CHECK(!warningMsgs.empty());
    CHECK(!infoMsgs.empty());
    CHECK(!logMsgs.empty());
}

TEST_CASE_FIXTURE(Fixture, "lsp.client.sendLogMessage rejects invalid type")
{

    std::string pluginPath = tempDir.write_child("test_log_invalid.luau", R"(
return {
    transformSource = function(source, context)
        local ok, err = pcall(function()
            lsp.client.sendLogMessage("invalid", "message")
        end)
        assert(not ok, "should have thrown")
        assert(string.find(err, "invalid message type") ~= nil, "wrong error: " .. tostring(err))
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_CASE_FIXTURE(Fixture, "print redirects to log with Info level")
{

    std::string pluginPath = tempDir.write_child("test_print.luau", R"(
return {
    transformSource = function(source, context)
        print("Hello from plugin")
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));

    // Verify print message was sent as Info with prefix
    auto infoMessages = getLogMessages(*client, lsp::MessageType::Info);
    bool foundMessage = false;
    for (const auto& msg : infoMessages)
    {
        if (msg.find("[Plugin") != std::string::npos && msg.find("Hello from plugin") != std::string::npos)
        {
            foundMessage = true;
            break;
        }
    }
    CHECK(foundMessage);
}

TEST_CASE_FIXTURE(Fixture, "print handles multiple arguments with tabs")
{

    std::string pluginPath = tempDir.write_child("test_print_multi.luau", R"(
return {
    transformSource = function(source, context)
        print("a", "b", "c")
        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));

    // Verify print message has tab-separated values
    auto infoMessages = getLogMessages(*client, lsp::MessageType::Info);
    bool foundMessage = false;
    for (const auto& msg : infoMessages)
    {
        if (msg.find("a\tb\tc") != std::string::npos)
        {
            foundMessage = true;
            break;
        }
    }
    CHECK(foundMessage);
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("PluginJsonAPI");

TEST_CASE_FIXTURE(Fixture, "lsp.json.deserialize parses simple values")
{

    std::string pluginPath = tempDir.write_child("test_json_simple.luau", R"(
return {
    transformSource = function(source, context)
        -- Test null
        local null_val = lsp.json.deserialize("null")
        assert(null_val == nil)

        -- Test booleans
        assert(lsp.json.deserialize("true") == true)
        assert(lsp.json.deserialize("false") == false)

        -- Test numbers
        assert(lsp.json.deserialize("42") == 42)
        assert(lsp.json.deserialize("3.14") == 3.14)
        assert(lsp.json.deserialize("-123") == -123)

        -- Test string
        assert(lsp.json.deserialize('"hello"') == "hello")
        assert(lsp.json.deserialize('"with spaces"') == "with spaces")

        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_CASE_FIXTURE(Fixture, "lsp.json.deserialize parses arrays")
{

    std::string pluginPath = tempDir.write_child("test_json_arrays.luau", R"(
return {
    transformSource = function(source, context)
        -- Test simple array
        local arr = lsp.json.deserialize("[1, 2, 3]")
        assert(type(arr) == "table")
        assert(#arr == 3)
        assert(arr[1] == 1)
        assert(arr[2] == 2)
        assert(arr[3] == 3)

        -- Test mixed array (note: #mixed is 3 because Lua length doesn't count trailing nil)
        local mixed = lsp.json.deserialize('[1, "hello", true, null]')
        assert(mixed[1] == 1)
        assert(mixed[2] == "hello")
        assert(mixed[3] == true)
        assert(mixed[4] == nil)

        -- Test empty array
        local empty = lsp.json.deserialize("[]")
        assert(type(empty) == "table")
        assert(#empty == 0)

        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_CASE_FIXTURE(Fixture, "lsp.json.deserialize parses objects")
{

    std::string pluginPath = tempDir.write_child("test_json_objects.luau", R"(
return {
    transformSource = function(source, context)
        -- Test simple object
        local obj = lsp.json.deserialize('{"name": "test", "count": 42}')
        assert(type(obj) == "table")
        assert(obj.name == "test")
        assert(obj.count == 42)

        -- Test object with various types
        local complex = lsp.json.deserialize('{"str": "hello", "num": 3.14, "bool": true, "null": null}')
        assert(complex.str == "hello")
        assert(complex.num == 3.14)
        assert(complex.bool == true)
        assert(complex.null == nil)

        -- Test empty object
        local empty = lsp.json.deserialize("{}")
        assert(type(empty) == "table")

        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_CASE_FIXTURE(Fixture, "lsp.json.deserialize parses nested structures")
{

    std::string pluginPath = tempDir.write_child("test_json_nested.luau", R"(
return {
    transformSource = function(source, context)
        -- Test nested object
        local nested = lsp.json.deserialize('{"user": {"name": "John", "age": 30}}')
        assert(nested.user.name == "John")
        assert(nested.user.age == 30)

        -- Test array of objects
        local items = lsp.json.deserialize('[{"id": 1}, {"id": 2}]')
        assert(#items == 2)
        assert(items[1].id == 1)
        assert(items[2].id == 2)

        -- Test object with array
        local data = lsp.json.deserialize('{"items": [1, 2, 3]}')
        assert(#data.items == 3)
        assert(data.items[1] == 1)

        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_CASE_FIXTURE(Fixture, "lsp.json.deserialize throws on invalid JSON")
{

    std::string pluginPath = tempDir.write_child("test_json_error.luau", R"(
return {
    transformSource = function(source, context)
        -- Test that invalid JSON throws
        local ok, err = pcall(lsp.json.deserialize, "invalid json")
        assert(ok == false)
        assert(type(err) == "string")
        assert(string.find(err, "JSON parse error") ~= nil)

        -- Test incomplete JSON
        local ok2, err2 = pcall(lsp.json.deserialize, '{"incomplete":')
        assert(ok2 == false)

        return nil
    end
}
)");

    PluginRuntime runtime(getWorkspaceNotNull(*this), Uri::file(pluginPath));
    REQUIRE(!runtime.load().has_value());

    PluginContext ctx{"test.luau", "test"};
    auto result = runtime.transformSource("local x = 1", ctx);

    CHECK(std::holds_alternative<std::vector<TextEdit>>(result));
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("PluginEnvironment");

TEST_CASE_FIXTURE(Fixture, "getEnvironmentForModule returns LSPPlugin for plugin files")
{

    std::string pluginPath = tempDir.write_child("my_plugin.luau", "return {}");

    // Configure plugin manager with the plugin
    workspace.fileResolver.pluginManager = std::make_unique<Luau::LanguageServer::Plugin::PluginManager>(
        client.get(), getWorkspaceNotNull(*this));
    workspace.fileResolver.pluginManager->configure({pluginPath});

    // Plugin files should return "LSPPlugin" environment
    auto env = workspace.fileResolver.getEnvironmentForModule(pluginPath);
    REQUIRE(env.has_value());
    CHECK(*env == "LSPPlugin");
}

TEST_CASE_FIXTURE(Fixture, "getEnvironmentForModule returns nullopt for non-plugin files")
{

    std::string pluginPath = tempDir.write_child("my_plugin.luau", "return {}");

    // Configure plugin manager with the plugin
    workspace.fileResolver.pluginManager = std::make_unique<Luau::LanguageServer::Plugin::PluginManager>(
        client.get(), getWorkspaceNotNull(*this));
    workspace.fileResolver.pluginManager->configure({pluginPath});

    // Non-plugin files should return nullopt
    auto env = workspace.fileResolver.getEnvironmentForModule("/some/other/file.luau");
    CHECK(!env.has_value());
}

TEST_CASE_FIXTURE(Fixture, "getEnvironmentForModule returns nullopt when no plugins configured")
{
    // No plugin manager configured
    workspace.fileResolver.pluginManager.reset();

    auto env = workspace.fileResolver.getEnvironmentForModule("/any/file.luau");
    CHECK(!env.has_value());
}

TEST_CASE_FIXTURE(Fixture, "isPluginFile matches loaded plugin paths")
{

    std::string pluginPath = tempDir.write_child("plugin.luau", "return {}");

    // Configure plugin manager
    workspace.fileResolver.pluginManager = std::make_unique<Luau::LanguageServer::Plugin::PluginManager>(
        client.get(), getWorkspaceNotNull(*this));
    workspace.fileResolver.pluginManager->configure({pluginPath});

    // Exact match
    CHECK(workspace.fileResolver.isPluginFile(pluginPath));

    // Different path
    CHECK_FALSE(workspace.fileResolver.isPluginFile("/path/to/other.luau"));
}

TEST_CASE_FIXTURE(Fixture, "reloadPlugins marks non-managed source nodes dirty")
{
    // Source: `--!strict\nlocal x: string = 1`
    // Plugin v1: rewrites "string" annotation (line 2, cols 10-15) to "number"
    // so `local x: string = 1` becomes `local x: number = 1` — no type error.
    // Plugin v2: no-op — type error is visible again.
    // The test verifies that after reloading the plugin, the non-managed file is
    // re-analysed with the new plugin and the error is detected.
    //
    // Column positions are 1-indexed. In `local x: string = 1`:
    //   l(1)o(2)c(3)a(4)l(5) (6)x(7):(8) (9)s(10)t(11)r(12)i(13)n(14)g(15)
    //   so "string" occupies columns 10-15, endColumn=16 (exclusive).
    std::string pluginPath = tempDir.write_child("plugin.luau", R"(
return {
    transformSource = function(source, context)
        return {
            {
                startLine = 2, startColumn = 10,
                endLine = 2, endColumn = 16,
                newText = "number"
            }
        }
    end
}
)");

    // Non-managed file: has a type error that plugin v1 fixes
    std::string filePath = tempDir.write_child("non_managed.luau", "--!strict\nlocal x: string = 1");
    auto fileUri = Uri::file(filePath);
    auto moduleName = workspace.fileResolver.getModuleName(fileUri);

    // Configure plugin v1 on the workspace
    workspace.fileResolver.pluginManager = std::make_unique<Luau::LanguageServer::Plugin::PluginManager>(
        client.get(), getWorkspaceNotNull(*this));
    workspace.fileResolver.pluginManager->configure({pluginPath});

    // Type-check with plugin v1: error is suppressed by the transformation
    auto result = workspace.frontend.check(moduleName);
    REQUIRE(result.errors.empty());
    REQUIRE_FALSE(workspace.frontend.isDirty(moduleName));

    // Update plugin to v2: no-op (no transformation applied)
    tempDir.write_child("plugin.luau", R"(
return {
    transformSource = function(source, context)
        return nil
    end
}
)");

    // Reload plugins — must mark non-managed source nodes dirty so they are re-analysed
    workspace.reloadPlugins();

    REQUIRE(workspace.frontend.isDirty(moduleName));

    // Re-check: plugin v2 does nothing, so the original type error is now visible
    result = workspace.frontend.check(moduleName);
    CHECK_FALSE(result.errors.empty());
}

TEST_SUITE_END();
