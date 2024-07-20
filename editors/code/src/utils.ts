import * as vscode from "vscode";
import * as path from "path";
import * as os from "os";

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

/// Expand ~ at the start of the path
export const resolvePath = (mainPath: string): string => {
  if (mainPath.startsWith("~/") || mainPath.startsWith("~\\")) {
    return path.join(os.homedir(), mainPath.slice(2));
  }
  return mainPath;
};
