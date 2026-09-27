import * as assert from "assert";
import * as fs from "fs";
import * as path from "path";
import * as vscode from "vscode";
import { activate, getDocUri } from "../helper";

async function waitFor(
  predicate: () => boolean,
  timeoutMs: number,
  intervalMs = 100,
): Promise<boolean> {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    if (predicate()) {
      return true;
    }
    await new Promise((resolve) => setTimeout(resolve, intervalMs));
  }
  return predicate();
}

// The extension falls back to a server binary bundled at `bin/server[.exe]` (relative to the
// extension root) when `luau-lsp.server.path` isn't configured. This test needs a real server to
// talk to, so it is skipped unless that binary has been built and placed there - see the
// `Luau.LanguageServer.CLI` build target in the top-level CLAUDE.md, e.g.:
//   cmake --build build --target Luau.LanguageServer.CLI --config Debug
//   cp build/luau-lsp editors/code/bin/server
function getBundledServerBinaryPath(): string {
  const binName = process.platform === "win32" ? "server.exe" : "server";
  return path.resolve(__dirname, "../../../bin", binName);
}

suite("Diagnostics Test Suite", () => {
  suiteSetup(function () {
    if (!fs.existsSync(getBundledServerBinaryPath())) {
      console.log(
        "Skipping: no bundled luau-lsp server binary found at " +
          getBundledServerBinaryPath(),
      );
      this.skip();
    }
  });

  // Reproduction for https://github.com/JohnnyMorganz/luau-lsp/issues/1019
  // "Diagnostics are still reported after closing a file (and workspace diagnostics disabled)"
  //
  // This exercises the real, production `vscode-languageclient` `LanguageClient` talking to a real
  // `luau-lsp` server process - not a mock - so it reproduces the bug exactly as a user would hit
  // it, purely through the VS Code client.
  //
  // Root cause (see `DiagnosticFeature`/`DiagnosticRequestor` in
  // node_modules/vscode-languageclient/lib/common/diagnostic.js):
  //   - luau-lsp always advertises `diagnosticProvider.workspaceDiagnostics: true` in its server
  //     capabilities at initialize time, regardless of the (dynamic, resource-scoped)
  //     `luau-lsp.diagnostics.workspace` setting (which testFixture/.vscode/settings.json disables,
  //     matching the issue title - and which is also the extension's default).
  //   - Because of that, when a document closes, `DiagnosticRequestor.forgetDocument` takes the
  //     "workspace diagnostics will clear this up eventually" branch instead of immediately
  //     deleting the document from its `vscode.DiagnosticCollection`.
  //   - But since `luau-lsp.diagnostics.workspace` is actually disabled, the server's workspace
  //     diagnostic pull never reports anything for the now-closed document, so the client never
  //     gets a chance to clear it - the stale diagnostics remain forever.
  test("stale diagnostics remain after closing a document with workspace diagnostics disabled", async function () {
    this.timeout(60000);

    const docUri = getDocUri("repro1019.luau");
    await activate(docUri);

    // Sanity check: while the file is open, it is diagnosed as expected (matches the first
    // screenshot on the issue).
    const gotInitialDiagnostics = await waitFor(
      () => vscode.languages.getDiagnostics(docUri).length > 0,
      30000,
    );
    assert.ok(
      gotInitialDiagnostics,
      "expected the open document to be diagnosed with a type error before closing it",
    );

    // Close the document. This sends a real `textDocument/didClose` notification through the
    // actual `LanguageClient`, exercising vscode-languageclient's production `DiagnosticFeature`.
    await vscode.commands.executeCommand("workbench.action.closeActiveEditor");

    // Give the client every reasonable chance to clear the stale diagnostics: its background
    // workspace-diagnostics pull loop re-runs every 2 seconds
    // (`DiagnosticRequestor.pullWorkspace` in vscode-languageclient), so wait comfortably longer
    // than that before checking.
    await new Promise((resolve) => setTimeout(resolve, 6000));

    const diagnosticsAfterClose = vscode.languages.getDiagnostics(docUri);

    // This assertion documents the CURRENT (buggy) behaviour described in #1019. Once the
    // upstream vscode-languageclient bug is fixed, this will start failing and should be inverted
    // to assert `diagnosticsAfterClose.length === 0`.
    assert.ok(
      diagnosticsAfterClose.length > 0,
      "expected stale diagnostics to remain after closing the document (reproduction of #1019); " +
        "if this fails, the bug has been fixed upstream and this assertion should be inverted",
    );
  });
});
