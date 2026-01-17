import * as vscode from "vscode";
import { ProvideOnTypeFormattingEditsSignature } from "vscode-languageclient";

export function isQuoteConversionEdit(edits: vscode.TextEdit[]): boolean {
  return edits.some((edit) => edit.newText === "`");
}

export function shouldHandleQuoteConversion(
  edits: vscode.TextEdit[] | null | undefined,
  ch: string,
): boolean {
  if (!edits || edits.length === 0) {
    return false;
  }
  if (ch !== "{") {
    return false;
  }
  return isQuoteConversionEdit(edits);
}

export async function onTypeFormattingMiddleware(
  document: vscode.TextDocument,
  position: vscode.Position,
  ch: string,
  options: vscode.FormattingOptions,
  token: vscode.CancellationToken,
  next: ProvideOnTypeFormattingEditsSignature,
): Promise<vscode.TextEdit[] | null | undefined> {
  const edits = await next(document, position, ch, options, token);

  if (!edits || !shouldHandleQuoteConversion(edits, ch)) {
    return edits;
  }

  const editor = vscode.window.activeTextEditor;
  if (!editor || editor.document !== document) {
    // Not the active document: don't bother with cursor shenanigans
    return edits;
  }

  // Apply text edits and restore cursor position
  const cursorPos = editor.selection.active;

  const workspaceEdit = new vscode.WorkspaceEdit();
  for (const edit of edits) {
    workspaceEdit.replace(document.uri, edit.range, edit.newText);
  }
  await vscode.workspace.applyEdit(workspaceEdit);

  const newSelection = new vscode.Selection(cursorPos, cursorPos);
  editor.selection = newSelection;

  // Return undefined to prevent double application
  return undefined;
}
