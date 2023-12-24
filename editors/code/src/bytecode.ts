/* eslint-disable @typescript-eslint/naming-convention */
import * as vscode from "vscode";
import {
  LanguageClient,
  RequestType,
  TextDocumentIdentifier,
} from "vscode-languageclient/node";

export const BYTECODE_SCHEME = "luau-bytecode";
export const COMPILER_REMARKS_SCHEME = "luau-remarks";

enum OptimizationLevel {
  None = 0,
  O1 = 1,
  O2 = 2,
}

export type BytecodeParams = {
  textDocument: TextDocumentIdentifier;
  optimizationLevel: OptimizationLevel;
};

export const BytecodeRequest = new RequestType<BytecodeParams, string, void>(
  "luau-lsp/bytecode"
);

export type CompilerRemarksParams = {
  textDocument: TextDocumentIdentifier;
  optimizationLevel: OptimizationLevel;
};

export const ComputeCompilerRemarksRequest = new RequestType<
  CompilerRemarksParams,
  string,
  void
>("luau-lsp/compilerRemarks");

export const getOptimizationLevel = async (): Promise<OptimizationLevel> => {
  const optimizationLevel = await vscode.window.showQuickPick(
    [
      {
        label: "O2",
        detail:
          "Includes optimizations that harm debuggability such as inlining",
        picked: true,
      },

      {
        label: "O1",
        detail:
          "Baseline optimization level that doesn't prevent debuggability",
      },
      {
        label: "None",
        detail: "No optimization",
      },
    ],
    {
      title: "Select Optimization Level",
      placeHolder: "Select optimization level",
    }
  );

  return optimizationLevel?.label === "None"
    ? OptimizationLevel.None
    : optimizationLevel?.label === "O1"
    ? OptimizationLevel.O1
    : optimizationLevel?.label === "O2"
    ? OptimizationLevel.O2
    : OptimizationLevel.O1;
};

let optimizationLevel = OptimizationLevel.O2;

// Based off https://github.com/rust-lang/rust-analyzer/blob/a24ede2066778f66b5b5e5aa7aa57a6d1be2063a/editors/code/src/commands.ts
// Licensed under MIT
const sleep = (ms: number) => {
  return new Promise((resolve) => setTimeout(resolve, ms));
};

const isLuauDocument = (document: vscode.TextDocument) => {
  return document.languageId === "luau" && document.uri.scheme === "file";
};

const isLuauEditor = (editor: vscode.TextEditor) => {
  return isLuauDocument(editor.document);
};

const activeLuauEditor = () => {
  const activeEditor = vscode.window.activeTextEditor;
  return activeEditor && isLuauEditor(activeEditor) ? activeEditor : undefined;
};

const getBytecodeInfo = (
  context: vscode.ExtensionContext,
  client: LanguageClient,
  command: string,
  scheme: string,
  fileName: string,
  requestType: RequestType<CompilerRemarksParams | BytecodeParams, string, void>
) => {
  const tdcp = new (class implements vscode.TextDocumentContentProvider {
    readonly uri = vscode.Uri.parse(`${scheme}://bytecode/${fileName}.luau`);
    readonly eventEmitter = new vscode.EventEmitter<vscode.Uri>();

    constructor() {
      vscode.workspace.onDidChangeTextDocument(
        this.onDidChangeTextDocument,
        this,
        context.subscriptions
      );
      vscode.window.onDidChangeActiveTextEditor(
        this.onDidChangeActiveTextEditor,
        this,
        context.subscriptions
      );
    }

    private onDidChangeTextDocument(event: vscode.TextDocumentChangeEvent) {
      if (isLuauDocument(event.document)) {
        // We need to order this after language server updates, but there's no API for that.
        // Hence, good old sleep().
        void sleep(10).then(() => this.eventEmitter.fire(this.uri));
      }
    }
    private onDidChangeActiveTextEditor(editor: vscode.TextEditor | undefined) {
      if (editor && isLuauEditor(editor)) {
        this.eventEmitter.fire(this.uri);
      }
    }

    async provideTextDocumentContent(uri: vscode.Uri): Promise<string> {
      const editor = activeLuauEditor();
      if (!editor) {
        return "";
      }

      const params = {
        textDocument: client.code2ProtocolConverter.asTextDocumentIdentifier(
          editor.document
        ),
        optimizationLevel,
      };

      return client.sendRequest(requestType, params);
    }

    get onDidChange(): vscode.Event<vscode.Uri> {
      return this.eventEmitter.event;
    }
  })();

  context.subscriptions.push(
    vscode.workspace.registerTextDocumentContentProvider(scheme, tdcp)
  );

  context.subscriptions.push(
    vscode.commands.registerTextEditorCommand(
      command,
      async (textEditor: vscode.TextEditor) => {
        if (isLuauEditor(textEditor)) {
          optimizationLevel = await getOptimizationLevel();

          const doc = await vscode.workspace.openTextDocument(tdcp.uri);
          tdcp.eventEmitter.fire(tdcp.uri);
          await vscode.window.showTextDocument(doc, {
            viewColumn: vscode.ViewColumn.Two,
            preserveFocus: true,
          });
        }
      }
    )
  );
};

export const registerComputeBytecode = (
  context: vscode.ExtensionContext,
  client: LanguageClient
) => {
  getBytecodeInfo(
    context,
    client,
    "luau-lsp.computeBytecode",
    BYTECODE_SCHEME,
    "bytecode",
    BytecodeRequest
  );
};

export const registerComputeCompilerRemarks = (
  context: vscode.ExtensionContext,
  client: LanguageClient
) => {
  getBytecodeInfo(
    context,
    client,
    "luau-lsp.computeCompilerRemarks",
    COMPILER_REMARKS_SCHEME,
    "compiler-remarks",
    ComputeCompilerRemarksRequest
  );
};
