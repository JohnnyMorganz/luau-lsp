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

  // Regression test for https://github.com/JohnnyMorganz/luau-lsp/issues/1019
  // "Diagnostics are still reported after closing a file (and workspace diagnostics disabled)"
  //
  // This exercises the real, production `vscode-languageclient` `LanguageClient` talking to a real
  // `luau-lsp` server process - not a mock - so it validates the fix exactly as a user would
  // experience it, purely through the VS Code client.
  //
  // Original root cause (see `DiagnosticFeature`/`DiagnosticRequestor` in
  // node_modules/vscode-languageclient/lib/common/diagnostic.js):
  //   - luau-lsp used to always advertise `diagnosticProvider.workspaceDiagnostics: true` in its
  //     static server capabilities at initialize time, regardless of the (dynamic,
  //     resource-scoped) `luau-lsp.diagnostics.workspace` setting (which
  //     testFixture/.vscode/settings.json disables, matching the issue title - and which is also
  //     the extension's default).
  //   - Because of that, when a document closed, `DiagnosticRequestor.forgetDocument` took the
  //     "workspace diagnostics will clear this up eventually" branch instead of immediately
  //     deleting the document from its `vscode.DiagnosticCollection`.
  //   - But since `luau-lsp.diagnostics.workspace` was actually disabled, the server's workspace
  //     diagnostic pull never reported anything for the now-closed document, so the client never
  //     got a chance to clear it - the stale diagnostics remained forever.
  //
  // Fix: the server now dynamically registers `textDocument/diagnostic` (when the client supports
  // dynamic registration for it) with `workspaceDiagnostics` reflecting the real, current setting,
  // instead of statically committing to `true`. See `LanguageServer::updateDiagnosticCapabilityRegistration`.
  test("diagnostics are cleared after closing a document with workspace diagnostics disabled", async function () {
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

    // The diagnostics should now be cleared promptly (this is the immediate-delete branch of
    // `DiagnosticRequestor.forgetDocument`, not the deferred workspace-pull branch), but poll for
    // a few seconds to avoid a flaky race with notification delivery.
    const diagnosticsCleared = await waitFor(
      () => vscode.languages.getDiagnostics(docUri).length === 0,
      6000,
    );

    assert.ok(
      diagnosticsCleared,
      "expected diagnostics to be cleared after closing the document (regression test for #1019)",
    );
  });
});
