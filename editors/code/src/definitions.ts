import * as vscode from "vscode";
import * as utils from "./utils";
import path from "path";

export const isExternalFile = (path: string) => {
  return path.startsWith("http://") || path.startsWith("https://");
};

export const outputLocationForDefinition = (
  context: vscode.ExtensionContext,
  packageName: string,
) => {
  return vscode.Uri.joinPath(
    context.globalStorageUri,
    "definitions",
    `${packageName}.d.luau`,
  );
};

export const outputLocationForDocumentation = (
  context: vscode.ExtensionContext,
  documentationPath: string,
) => {
  return vscode.Uri.joinPath(
    context.globalStorageUri,
    "documentation",
    path.basename(documentationPath),
  );
};

export const anyFileIsMissing = async (uris: vscode.Uri[]) => {
  return (
    await Promise.all(uris.map(async (uri) => await utils.exists(uri)))
  ).includes(false);
};

/**
 * Check whether we should re-fetch definitions files from external sources.
 * We configure to re-fetch once a day.
 *
 * NOTE: This function has a side-effect, you should call once and re-use the result throughout your code.
 * Calling this function twice will return false the second time.
 */
export const shouldFetchDefinitions = (context: vscode.ExtensionContext) => {
  const lastUpdate = context.globalState.get<number>("last-api-update") ?? 0;
  const currentTimeMs = Date.now();

  if (currentTimeMs - lastUpdate > 1000 * 60 * 60 * 24) {
    context.globalState.update("last-api-update", currentTimeMs);
    return true;
  } else {
    return false;
  }
};

export interface DownloadFileDefinition {
  url: string;
  outputUri: vscode.Uri;
}

/**
 * Downloads a set of files from external sources to the provided locations.
 */
export const downloadExternalFiles = async (
  files: DownloadFileDefinition[],
) => {
  return vscode.window.withProgress(
    {
      location: vscode.ProgressLocation.Window,
      title: "Luau: Updating API Definitions",
      cancellable: false,
    },
    async () => {
      return Promise.all(
        files.map((info) =>
          fetch(info.url)
            .then((r) => {
              if (!r.ok) {
                return Promise.reject(
                  `fetching documentation error: ${r.status} ${r.statusText}`,
                );
              }

              return r.arrayBuffer();
            })
            .then((data) =>
              vscode.workspace.fs.writeFile(
                info.outputUri,
                new Uint8Array(data),
              ),
            ),
        ),
      ).catch((err) =>
        vscode.window.showErrorMessage(
          `Failed to retrieve API information: ${err}, cause: ${err.cause}`,
        ),
      );
    },
  );
};
