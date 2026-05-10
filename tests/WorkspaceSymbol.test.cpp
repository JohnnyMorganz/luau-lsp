#include "doctest.h"
#include "Fixture.h"

#include <algorithm>

namespace
{
const lsp::WorkspaceSymbol* findSymbol(const std::vector<lsp::WorkspaceSymbol>& symbols, const std::string& name)
{
    for (const auto& s : symbols)
        if (s.name == name)
            return &s;
    return nullptr;
}

std::vector<std::string> sortedNames(const std::vector<lsp::WorkspaceSymbol>& symbols)
{
    std::vector<std::string> names;
    names.reserve(symbols.size());
    for (const auto& s : symbols)
        names.push_back(s.name);
    std::sort(names.begin(), names.end());
    return names;
}
} // namespace

TEST_SUITE_BEGIN("WorkspaceSymbol");

TEST_CASE_FIXTURE(Fixture, "does_not_invalidate_iterator_when_source_becomes_unreadable")
{
    // Reproduces a crash in WorkspaceFolder::workspaceSymbol where the loop
    // iterates `frontend.sourceModules` while calling `frontend.parse(moduleName)`
    // inside the body. `parse` -> `parseGraph` -> `getSourceNode` mutates
    // `sourceModules` (erases when source becomes unreadable, inserts when a
    // new dependency is discovered), invalidating the range-for iterator.
    //
    // Setup: open several documents so iteration has multiple steps, then
    // close them all. closeTextDocument removes them from managedFiles and
    // marks the source modules dirty. The files are not on disk, so the next
    // parse() hits the `sourceModules.erase(name)` path inside getSourceNode,
    // invalidating the for-loop iterator. Under ASAN this surfaces as
    // heap-use-after-free.
    auto a = newDocument("a.luau", "local x = 1");
    auto b = newDocument("b.luau", "local x = 2");
    auto c = newDocument("c.luau", "local x = 3");
    auto d = newDocument("d.luau", "local x = 4");
    auto e = newDocument("e.luau", "local x = 5");

    workspace.closeTextDocument(a);
    workspace.closeTextDocument(b);
    workspace.closeTextDocument(c);
    workspace.closeTextDocument(d);
    workspace.closeTextDocument(e);

    lsp::WorkspaceSymbolParams params;
    workspace.workspaceSymbol(params);
}

TEST_CASE_FIXTURE(Fixture, "empty_query_returns_all_symbols")
{
    newDocument("a.luau", R"(
        local function foo() end
        local function bar() end
    )");

    lsp::WorkspaceSymbolParams params;
    auto result = workspace.workspaceSymbol(params);

    REQUIRE(result);
    CHECK_EQ(sortedNames(*result), std::vector<std::string>{"bar", "foo"});
}

TEST_CASE_FIXTURE(Fixture, "matches_symbol_when_query_appears_at_start_of_name")
{
    // Regression: matchesQuery used to convert string::find's return value
    // directly to bool, treating "found at index 0" as "no match".
    newDocument("a.luau", "local function foo() end");

    lsp::WorkspaceSymbolParams params;
    params.query = "foo";
    auto result = workspace.workspaceSymbol(params);

    REQUIRE(result);
    REQUIRE_EQ(result->size(), 1);
    CHECK_EQ(result->at(0).name, "foo");
}

TEST_CASE_FIXTURE(Fixture, "query_filter_excludes_non_matching_symbols")
{
    // Regression: matchesQuery used to convert string::find's return value
    // directly to bool, treating npos (not found) as truthy.
    newDocument("a.luau", R"(
        local function foo() end
        local function bar() end
    )");

    lsp::WorkspaceSymbolParams params;
    params.query = "foo";
    auto result = workspace.workspaceSymbol(params);

    REQUIRE(result);
    REQUIRE_EQ(result->size(), 1);
    CHECK_EQ(result->at(0).name, "foo");
}

TEST_CASE_FIXTURE(Fixture, "query_matches_case_insensitively")
{
    newDocument("a.luau", "local function MyFunction() end");

    lsp::WorkspaceSymbolParams params;
    params.query = "myfunc";
    auto result = workspace.workspaceSymbol(params);

    REQUIRE(result);
    REQUIRE_EQ(result->size(), 1);
    CHECK_EQ(result->at(0).name, "MyFunction");
}

TEST_CASE_FIXTURE(Fixture, "reports_correct_symbol_kinds")
{
    newDocument("a.luau", R"(
        local x = 1
        type Tee = string
        function fn() end
        local function localFn() end
        local tbl = {}
        function tbl:method() end
    )");

    lsp::WorkspaceSymbolParams params;
    auto result = workspace.workspaceSymbol(params);
    REQUIRE(result);

    auto x = findSymbol(*result, "x");
    REQUIRE(x);
    CHECK(x->kind == lsp::SymbolKind::Variable);

    auto tee = findSymbol(*result, "Tee");
    REQUIRE(tee);
    CHECK(tee->kind == lsp::SymbolKind::Interface);

    auto fn = findSymbol(*result, "fn");
    REQUIRE(fn);
    CHECK(fn->kind == lsp::SymbolKind::Function);

    auto localFn = findSymbol(*result, "localFn");
    REQUIRE(localFn);
    CHECK(localFn->kind == lsp::SymbolKind::Function);

    auto method = findSymbol(*result, "tbl:method");
    REQUIRE(method);
    CHECK(method->kind == lsp::SymbolKind::Method);
}

TEST_CASE_FIXTURE(Fixture, "multi_variable_local_yields_multiple_symbols")
{
    newDocument("a.luau", "local a, b, c = 1, 2, 3");

    lsp::WorkspaceSymbolParams params;
    auto result = workspace.workspaceSymbol(params);

    REQUIRE(result);
    CHECK_EQ(sortedNames(*result), std::vector<std::string>{"a", "b", "c"});
}

TEST_CASE_FIXTURE(Fixture, "returns_symbols_from_multiple_files")
{
    newDocument("a.luau", "local foo = 1");
    newDocument("b.luau", "local bar = 2");

    lsp::WorkspaceSymbolParams params;
    auto result = workspace.workspaceSymbol(params);

    REQUIRE(result);
    CHECK_EQ(sortedNames(*result), std::vector<std::string>{"bar", "foo"});
}

TEST_SUITE_END();
