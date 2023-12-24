/* eslint-disable @typescript-eslint/naming-convention */
import * as vscode from "vscode";
import {
  LanguageClient,
  RequestType,
  TextDocumentIdentifier,
} from "vscode-languageclient/node";

export const COMPILER_REMARKS_SCHEME = "luau-remarks";

enum OptimizationLevel {
  None = 0,
  O1 = 1,
  O2 = 2,
}

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

export const registerCompilerRemarks = (
  context: vscode.ExtensionContext,
  client: LanguageClient
) => {
  let compilerRemarksContents: string | undefined;

  context.subscriptions.push(
    vscode.workspace.registerTextDocumentContentProvider(
      COMPILER_REMARKS_SCHEME,
      new (class implements vscode.TextDocumentContentProvider {
        provideTextDocumentContent(uri: vscode.Uri): string {
          return (
            compilerRemarksContents ?? "error: compiler remarks not computed"
          );
        }
      })()
    )
  );

  context.subscriptions.push(
    vscode.commands.registerTextEditorCommand(
      "luau-lsp.computeCompilerRemarks",
      async (textEditor: vscode.TextEditor) => {
        compilerRemarksContents = undefined;
        if (!client) {
          return;
        }

        const document = textEditor.document;
        if (
          document.languageId === "luau" &&
          document.uri.scheme !== COMPILER_REMARKS_SCHEME
        ) {
          const params = {
            textDocument:
              client.code2ProtocolConverter.asTextDocumentIdentifier(document),
            optimizationLevel: await getOptimizationLevel(),
          };
          compilerRemarksContents = await client.sendRequest(
            ComputeCompilerRemarksRequest,
            params
          );

          const uri = document.uri.with({
            scheme: COMPILER_REMARKS_SCHEME,
          });

          const doc = await vscode.workspace.openTextDocument(uri);
          await vscode.window.showTextDocument(doc, { preview: true });
        }
      }
    )
  );
};
