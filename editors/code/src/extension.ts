import * as vscode from "vscode";
import * as os from "os";
import * as fs from "fs";
import {
  Executable,
  ServerOptions,
  LanguageClient,
  LanguageClientOptions,
  Trace,
} from "vscode-languageclient/node";

export const fileExists = (path: vscode.Uri | string): Thenable<boolean> => {
  const uri = path instanceof vscode.Uri ? path : vscode.Uri.file(path);
  return vscode.workspace.fs.stat(uri).then(
    () => true,
    () => false
  );
};

const findBuiltServer = (context: vscode.ExtensionContext) => {
  const platform = os.platform();
  const path = vscode.Uri.joinPath(
    context.extensionUri,
    "bin",
    platform,
    `luau-lsp${platform === "win32" ? ".exe" : ""}`
  );

  //   if (!fileExists(path)) {
  //     vscode.window.showErrorMessage("Luau LSP is currently not supported on your platform");
  //     return undefined
  //   }

  if (platform !== "win32") {
    fs.chmodSync(path.fsPath, "777");
  }
  return path;
};

export function activate(context: vscode.ExtensionContext) {
  console.log("Luau LSP activated");

  const run: Executable = {
    command: findBuiltServer(context).fsPath,
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
