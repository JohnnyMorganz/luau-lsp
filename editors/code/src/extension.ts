import * as vscode from "vscode";
import * as os from "os";
import fetch from "node-fetch";
import {
  Executable,
  ServerOptions,
  LanguageClient,
  LanguageClientOptions,
} from "vscode-languageclient/node";
import { spawn } from "child_process";

const CURRENT_VERSION_TXT =
  "https://raw.githubusercontent.com/CloneTrooper1019/Roblox-Client-Tracker/roblox/version.txt";
const GLOBAL_TYPES_DEFINITION =
  "https://raw.githubusercontent.com/JohnnyMorganz/luau-analyze-rojo/master/globalTypes.d.lua";
const API_DOCS =
  "https://raw.githubusercontent.com/MaximumADHD/Roblox-Client-Tracker/roblox/api-docs/en-us.json";
const CURRENT_FFLAGS =
  "https://clientsettingscdn.roblox.com/v1/settings/application?applicationName=PCDesktopClient";

type FFlags = Record<string, string>;
type FFlagsEndpoint = { applicationSettings: FFlags };

const globalTypesUri = (context: vscode.ExtensionContext) => {
  return vscode.Uri.joinPath(context.globalStorageUri, "globalTypes.d.lua");
};

const apiDocsUri = (context: vscode.ExtensionContext) => {
  return vscode.Uri.joinPath(context.globalStorageUri, "api-docs.json");
};

const exists = (uri: vscode.Uri): Thenable<boolean> => {
  return vscode.workspace.fs.stat(uri).then(
    () => true,
    () => false
  );
};

const updateApiInfo = async (context: vscode.ExtensionContext) => {
  try {
    const latestVersion = await fetch(CURRENT_VERSION_TXT).then((r) =>
      r.text()
    );
    const currentVersion = context.globalState.get<string>(
      "current-api-version"
    );
    const mustUpdate =
      !(await exists(globalTypesUri(context))) ||
      !(await exists(apiDocsUri(context)));

    if (!currentVersion || currentVersion !== latestVersion || mustUpdate) {
      context.globalState.update("current-api-version", latestVersion);
      return vscode.window.withProgress(
        {
          location: vscode.ProgressLocation.Notification,
          title: "Updating API",
          cancellable: false,
        },
        async () => {
          return Promise.all([
            fetch(GLOBAL_TYPES_DEFINITION)
              .then((r) => r.arrayBuffer())
              .then((data) =>
                vscode.workspace.fs.writeFile(
                  globalTypesUri(context),
                  new Uint8Array(data)
                )
              ),
            fetch(API_DOCS)
              .then((r) => r.arrayBuffer())
              .then((data) =>
                vscode.workspace.fs.writeFile(
                  apiDocsUri(context),
                  new Uint8Array(data)
                )
              ),
          ]);
        }
      );
    }
  } catch (err) {
    vscode.window.showErrorMessage(
      "Failed to retrieve API information: " + err
    );
  }
};

const getFFlags = async () => {
  try {
    return vscode.window.withProgress(
      {
        location: vscode.ProgressLocation.Window,
        title: "Fetching FFlags",
        cancellable: false,
      },
      () =>
        fetch(CURRENT_FFLAGS)
          .then((r) => r.json() as Promise<FFlagsEndpoint>)
          .then((r) => r.applicationSettings)
    );
  } catch (err) {
    vscode.window.showErrorMessage(
      "Failed to fetch current Luau FFlags: " + err
    );
  }
};

export async function activate(context: vscode.ExtensionContext) {
  console.log("Luau LSP activated");

  await updateApiInfo(context);
  const args = [
    `--definitions=${globalTypesUri(context).fsPath}`,
    `--docs=${apiDocsUri(context).fsPath}`,
  ];

  // Handle FFlags
  const fflags: FFlags = {};
  const fflagsConfig = vscode.workspace.getConfiguration("luau-lsp.fflags");

  // Sync FFlags with upstream
  if (fflagsConfig.get<boolean>("sync")) {
    const currentFlags = await getFFlags();
    if (currentFlags) {
      for (const [name, value] of Object.entries(currentFlags)) {
        if (name.startsWith("FFlagLuau")) {
          fflags[name.substring(5)] = value; // Remove the "FFlag" part from the name
        }
      }
    }
  }

  // Handle overrides
  const overridenFFlags = fflagsConfig.get<FFlags>("override");
  if (overridenFFlags) {
    for (const [name, value] of Object.entries(overridenFFlags)) {
      fflags[name] = value;
    }
  }

  console.log(fflags);

  // Pass FFlags as arguments
  for (const [name, value] of Object.entries(fflags)) {
    args.push(`--flag:${name}=${value}`);
  }

  const run: Executable = {
    command: vscode.Uri.joinPath(
      context.extensionUri,
      "bin",
      os.platform() === "win32" ? "server.exe" : "server"
    ).fsPath,
    args,
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
    args,
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
    "luau",
    "Luau LSP",
    serverOptions,
    clientOptions
  );

  client.onReady().then(() => {
    client.onNotification("$/command", (params) => {
      vscode.commands.executeCommand(params.command, params.data);
    });
  });

  // Register automatic sourcemap regenerate
  // TODO: maybe we should move this to the server in future
  const updateSourceMap = (workspaceFolder: vscode.WorkspaceFolder) => {
    const config = vscode.workspace.getConfiguration("luau-lsp.sourcemap");
    if (!config.get<boolean>("autogenerate")) {
      // TODO: maybe we should disconnect the event instead of early returning? Bit more messy
      return;
    }

    client.info(
      `Regenerating sourcemap for ${
        workspaceFolder.name
      } (${workspaceFolder.uri.toString(true)})`
    );
    const args = [
      "sourcemap",
      config.get<string>("rojoProjectFile") ?? "default.project.json",
      "--output",
      "sourcemap.json",
    ];
    if (config.get<boolean>("includeNonScripts")) {
      args.push("--include-non-scripts");
    }

    const child = spawn("rojo", args, { cwd: workspaceFolder.uri.fsPath });

    const onFailEvent = (err: any) => {
      client.warn(
        `Failed to update sourcemap for ${
          workspaceFolder.name
        } (${workspaceFolder.uri.toString(true)}): ` + err
      );
    };
    child.stderr.on("data", onFailEvent);
    child.on("error", onFailEvent);
  };

  const listener = (e: vscode.FileCreateEvent | vscode.FileDeleteEvent) => {
    if (e.files.length === 0) {
      return;
    }
    const workspaceFolder = vscode.workspace.getWorkspaceFolder(e.files[0]);
    if (!workspaceFolder) {
      return;
    }
    return updateSourceMap(workspaceFolder);
  };

  context.subscriptions.push(vscode.workspace.onDidCreateFiles(listener));
  context.subscriptions.push(vscode.workspace.onDidDeleteFiles(listener));
  context.subscriptions.push(
    vscode.workspace.onDidRenameFiles((e) => {
      if (e.files.length === 0) {
        return;
      }
      const workspaceFolder = vscode.workspace.getWorkspaceFolder(
        e.files[0].oldUri
      );
      if (!workspaceFolder) {
        return;
      }
      return updateSourceMap(workspaceFolder);
    })
  );
  context.subscriptions.push(
    vscode.workspace.onDidChangeWorkspaceFolders((e) => {
      for (const folder of e.added) {
        updateSourceMap(folder);
      }
    })
  );
  context.subscriptions.push(
    vscode.workspace.onDidChangeConfiguration((e) => {
      if (
        e.affectsConfiguration("luau-lsp.autogenerateSourcemap") ||
        e.affectsConfiguration("luau-lsp.includeNonScriptsInSourcemap") ||
        e.affectsConfiguration("luau-lsp.rojoProjectFile")
      ) {
        if (vscode.workspace.workspaceFolders) {
          for (const folder of vscode.workspace.workspaceFolders) {
            updateSourceMap(folder);
          }
        }
      } else if (e.affectsConfiguration("luau-lsp.fflags")) {
        vscode.window
          .showInformationMessage(
            "Luau FFlags have been changed, reload your workspace for this to take effect.",
            "Reload Workspace"
          )
          .then((command) => {
            if (command === "Reload Workspace") {
              vscode.commands.executeCommand("workbench.action.reloadWindow");
            }
          });
      }
    })
  );

  if (vscode.workspace.workspaceFolders) {
    for (const folder of vscode.workspace.workspaceFolders) {
      updateSourceMap(folder);
    }
  }

  console.log("LSP Setup");
  context.subscriptions.push(client.start());
}

export function deactivate() {}
