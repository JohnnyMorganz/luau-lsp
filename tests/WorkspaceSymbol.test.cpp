#include "doctest.h"
#include "Fixture.h"

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

TEST_SUITE_END();
