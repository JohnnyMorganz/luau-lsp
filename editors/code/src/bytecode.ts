/* eslint-disable @typescript-eslint/naming-convention */
import * as vscode from "vscode";
import {
  LanguageClient,
  RequestType,
  TextDocumentIdentifier,
} from "vscode-languageclient/node";

export const BYTECODE_SCHEME = "luau-bytecode";
export const COMPILER_REMARKS_SCHEME = "luau-remarks";
export const CODEGEN_SCHEME = "luau-codegen";

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
  "luau-lsp/bytecode",
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

export type CodeGenTarget =
  | "host"
  | "a64"
  | "a64_nofeatures"
  | "x64_windows"
  | "x64_systemv";

export type CodeGenParams = {
  textDocument: TextDocumentIdentifier;
  optimizationLevel: OptimizationLevel;
  codeGenTarget: CodeGenTarget;
};

export const ComputeCodeGenRequest = new RequestType<
  CodeGenParams,
  string,
  void
>("luau-lsp/codeGen");

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
    },
  );

  return optimizationLevel?.label === "None"
    ? OptimizationLevel.None
    : optimizationLevel?.label === "O1"
      ? OptimizationLevel.O1
      : optimizationLevel?.label === "O2"
        ? OptimizationLevel.O2
        : OptimizationLevel.O1;
};

export const getCodeGenTarget = async (): Promise<CodeGenTarget> => {
  const codeGenTargetSelection = await vscode.window.showQuickPick(
    [
      {
        label: "host",
        detail: "Host target",
        picked: true,
      },

      {
        label: "a64",
        detail: "Arm64",
      },
      {
        label: "a64_nofeatures",
        detail: "Arm64 (No Features)",
      },
      {
        label: "x64_windows",
        detail: "x64 Windows",
      },
      {
        label: "x64_systemv",
        detail: "x64 SystemV",
      },
    ],
    {
      title: "Select CodeGen target",
      placeHolder: "Select CodeGen target",
    },
  );

  return (codeGenTargetSelection?.label as CodeGenTarget) ?? "host";
};

let optimizationLevel = OptimizationLevel.O2;
let codeGenTarget: CodeGenTarget = "host";

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
  requestType: RequestType<
    CompilerRemarksParams | BytecodeParams | CodeGenParams,
    string,
    void
  >,
) => {
  const tdcp = new (class implements vscode.TextDocumentContentProvider {
    readonly uri = vscode.Uri.parse(`${scheme}://bytecode/${fileName}.luau`);
    readonly eventEmitter = new vscode.EventEmitter<vscode.Uri>();

    constructor() {
      vscode.workspace.onDidChangeTextDocument(
        this.onDidChangeTextDocument,
        this,
        context.subscriptions,
      );
      vscode.window.onDidChangeActiveTextEditor(
        this.onDidChangeActiveTextEditor,
        this,
        context.subscriptions,
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
          editor.document,
        ),
        optimizationLevel,
        codeGenTarget,
      };

      return client.sendRequest(requestType, params);
    }

    get onDidChange(): vscode.Event<vscode.Uri> {
      return this.eventEmitter.event;
    }
  })();

  return [
    vscode.workspace.registerTextDocumentContentProvider(scheme, tdcp),
    vscode.commands.registerTextEditorCommand(
      command,
      async (textEditor: vscode.TextEditor) => {
        if (isLuauEditor(textEditor)) {
          optimizationLevel = await getOptimizationLevel();

          if (command === "luau-lsp.computeCodeGen") {
            codeGenTarget = await getCodeGenTarget();
          }

          const doc = await vscode.workspace.openTextDocument(tdcp.uri);
          tdcp.eventEmitter.fire(tdcp.uri);
          await vscode.window.showTextDocument(doc, {
            viewColumn: vscode.ViewColumn.Two,
            preserveFocus: true,
          });
        }
      },
    ),
  ];
};

export const registerComputeBytecode = (
  context: vscode.ExtensionContext,
  client: LanguageClient,
): vscode.Disposable[] => {
  return getBytecodeInfo(
    context,
    client,
    "luau-lsp.computeBytecode",
    BYTECODE_SCHEME,
    "bytecode",
    BytecodeRequest,
  );
};

export const registerComputeCompilerRemarks = (
  context: vscode.ExtensionContext,
  client: LanguageClient,
): vscode.Disposable[] => {
  return getBytecodeInfo(
    context,
    client,
    "luau-lsp.computeCompilerRemarks",
    COMPILER_REMARKS_SCHEME,
    "compiler-remarks",
    ComputeCompilerRemarksRequest,
  );
};

export const registerComputeCodeGen = (
  context: vscode.ExtensionContext,
  client: LanguageClient,
): vscode.Disposable[] => {
  return getBytecodeInfo(
    context,
    client,
    "luau-lsp.computeCodeGen",
    CODEGEN_SCHEME,
    "codeGen",
    ComputeCodeGenRequest,
  );
};
