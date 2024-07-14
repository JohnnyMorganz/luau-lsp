import * as vscode from "vscode";
import * as path from "path";

export const basenameUri = (uri: vscode.Uri): string => {
  return path.basename(uri.fsPath);
};

export const exists = (uri: vscode.Uri): Thenable<boolean> => {
  return vscode.workspace.fs.stat(uri).then(
    () => true,
    () => false,
  );
};

export const resolveUri = (uri: vscode.Uri, ...paths: string[]): vscode.Uri => {
  return vscode.Uri.file(path.resolve(uri.fsPath, ...paths));
};
