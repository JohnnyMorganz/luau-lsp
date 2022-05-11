import path = require("path");
import * as vscode from "vscode";
import {
  Executable,
  ServerOptions,
  LanguageClient,
  LanguageClientOptions,
} from "vscode-languageclient/node";

export function activate(context: vscode.ExtensionContext) {
  console.log("Luau LSP activated");

  const serverPath = vscode.Uri.joinPath(
    context.extensionUri,
    "..",
    "..",
    "build",
    "Debug",
    "luau-lsp.exe"
  ).fsPath;

  const run: Executable = {
    command: serverPath,
  };
  const serverOptions: ServerOptions = { run, debug: run };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [
      { scheme: "file", language: "lua" },
      { scheme: "file", language: "luau" },
    ],
    synchronize: {
      fileEvents: vscode.workspace.createFileSystemWatcher("**/.luaurc"),
    },
    diagnosticCollectionName: "luau",
  };

  const client = new LanguageClient(
    "luau-lsp",
    "Luau LSP",
    serverOptions,
    clientOptions
  );

  console.log("LSP Setup");
  context.subscriptions.push(client.start());
}

export function deactivate() {}
