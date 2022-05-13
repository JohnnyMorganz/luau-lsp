import * as vscode from "vscode";
import * as os from "os";
import {
  Executable,
  ServerOptions,
  LanguageClient,
  LanguageClientOptions,
  Trace,
} from "vscode-languageclient/node";

export function activate(context: vscode.ExtensionContext) {
  console.log("Luau LSP activated");

  const run: Executable = {
    command: vscode.Uri.joinPath(
      context.extensionUri,
      "bin",
      os.platform() === "win32" ? "server.exe" : "server"
    ).fsPath,
  };

  // If debugging, run the locally build extension
  const debug: Executable = {
    command: vscode.Uri.joinPath(
      context.extensionUri,
      "..",
      "..",
      "build",
      "Debug",
      "luau-lsp.exe"
    ).fsPath,
  };

  const serverOptions: ServerOptions = { run, debug };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [
      { scheme: "file", language: "lua" },
      { scheme: "file", language: "luau" },
    ],
    synchronize: {
      fileEvents: [
        vscode.workspace.createFileSystemWatcher("**/.luaurc"),
        vscode.workspace.createFileSystemWatcher("**/sourcemap.json"),
      ],
    },
    diagnosticCollectionName: "luau",
  };

  const client = new LanguageClient(
    "luau-lsp",
    "Luau LSP",
    serverOptions,
    clientOptions
  );
  client.trace = Trace.Messages;

  console.log("LSP Setup");
  context.subscriptions.push(client.start());
}

export function deactivate() {}
