import * as vscode from "vscode";
import {
  LanguageClient,
  RequestType,
  TextDocumentIdentifier,
} from "vscode-languageclient/node";

export const INTERNAL_SOURCE_SCHEME = "luau-internal-source";

export type InternalSourceParams = {
  textDocument: TextDocumentIdentifier;
};

export const InternalSourceRequest = new RequestType<
  InternalSourceParams,
  string,
  void
>("luau-lsp/debug/viewInternalSource");

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

// Based off https://github.com/rust-lang/rust-analyzer/blob/a24ede2066778f66b5b5e5aa7aa57a6d1be2063a/editors/code/src/commands.ts
// Licensed under MIT
const sleep = (ms: number) => {
  return new Promise((resolve) => setTimeout(resolve, ms));
};

export const registerViewInternalSource = (
  context: vscode.ExtensionContext,
  client: LanguageClient,
): vscode.Disposable[] => {
  const tdcp = new (class implements vscode.TextDocumentContentProvider {
    readonly uri = vscode.Uri.parse(
      `${INTERNAL_SOURCE_SCHEME}://internal-source/source.luau`,
    );
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

    async provideTextDocumentContent(_uri: vscode.Uri): Promise<string> {
      const editor = activeLuauEditor();
      if (!editor) {
        return "";
      }

      const params: InternalSourceParams = {
        textDocument: client.code2ProtocolConverter.asTextDocumentIdentifier(
          editor.document,
        ),
      };

      return client.sendRequest(InternalSourceRequest, params);
    }

    get onDidChange(): vscode.Event<vscode.Uri> {
      return this.eventEmitter.event;
    }
  })();

  return [
    vscode.workspace.registerTextDocumentContentProvider(
      INTERNAL_SOURCE_SCHEME,
      tdcp,
    ),
    vscode.commands.registerTextEditorCommand(
      "luau-lsp.debug.viewInternalSource",
      async (textEditor: vscode.TextEditor) => {
        if (isLuauEditor(textEditor)) {
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
