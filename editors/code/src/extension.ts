import * as vscode from "vscode";
import * as os from "os";
import * as path from "path";
import fetch from "node-fetch";
import {
  Executable,
  ServerOptions,
  LanguageClient,
  LanguageClientOptions,
} from "vscode-languageclient/node";

import spawn from "./spawn";

let client: LanguageClient;

const CURRENT_VERSION_TXT =
  "https://raw.githubusercontent.com/CloneTrooper1019/Roblox-Client-Tracker/roblox/version.txt";
const GLOBAL_TYPES_DEFINITION =
  "https://raw.githubusercontent.com/JohnnyMorganz/luau-lsp/master/scripts/globalTypes.d.lua";
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

const basenameUri = (uri: vscode.Uri): string => {
  return path.basename(uri.fsPath);
};

const resolveUri = (uri: vscode.Uri, ...paths: string[]): vscode.Uri => {
  return vscode.Uri.file(path.resolve(uri.fsPath, ...paths));
};

const downloadApiDefinitions = async (context: vscode.ExtensionContext) => {
  try {
    return vscode.window.withProgress(
      {
        location: vscode.ProgressLocation.Window,
        title: "Luau: Updating API Definitions",
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
  } catch (err) {
    vscode.window.showErrorMessage(
      "Failed to retrieve API information: " + err
    );
  }
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
      return downloadApiDefinitions(context);
    }
  } catch (err) {
    vscode.window.showWarningMessage(
      "Failed to retrieve API information: " + err
    );
  }
};

const getFFlags = async () => {
  return vscode.window.withProgress(
    {
      location: vscode.ProgressLocation.Window,
      title: "Luau: Fetching FFlags",
      cancellable: false,
    },
    () =>
      fetch(CURRENT_FFLAGS)
        .then((r) => r.json() as Promise<FFlagsEndpoint>)
        .then((r) => r.applicationSettings)
  );
};

const updateSourceMap = async (workspaceFolder: vscode.WorkspaceFolder) => {
  const config = vscode.workspace.getConfiguration(
    "luau-lsp.sourcemap",
    workspaceFolder
  );
  if (!config.get<boolean>("enabled") || !config.get<boolean>("autogenerate")) {
    // TODO: maybe we should disconnect the event instead of early returning? Bit more messy
    return;
  }

  // Check if the project file exists
  let projectFile =
    config.get<string>("rojoProjectFile") ?? "default.project.json";
  const projectFileUri = resolveUri(workspaceFolder.uri, projectFile);

  if (!(await exists(projectFileUri))) {
    // Search if there is a *.project.json file present in this workspace.
    const foundProjectFiles = await vscode.workspace.findFiles(
      new vscode.RelativePattern(workspaceFolder.uri, "*.project.json")
    );

    if (foundProjectFiles.length === 0) {
      vscode.window.showWarningMessage(
        `Unable to find project file ${projectFile}. Please configure a file in settings`
      );
      return;
    } else if (foundProjectFiles.length === 1) {
      const fileName = basenameUri(foundProjectFiles[0]);
      const option = await vscode.window.showWarningMessage(
        `Unable to find project file ${projectFile}. We found ${fileName} available`,
        `Set project file to ${fileName}`,
        "Cancel"
      );

      if (option === `Set project file to ${fileName}`) {
        config.update("rojoProjectFile", fileName);
        projectFile = fileName;
      } else {
        return;
      }
    } else {
      const option = await vscode.window.showWarningMessage(
        `Unable to find project file ${projectFile}. We found ${foundProjectFiles.length} files available`,
        "Select project file",
        "Cancel"
      );
      if (option === "Select project file") {
        const files = foundProjectFiles.map((file) => basenameUri(file));
        const selectedFile = await vscode.window.showQuickPick(files);
        if (selectedFile) {
          config.update("rojoProjectFile", selectedFile);
          projectFile = selectedFile;
        } else {
          return;
        }
      } else {
        return;
      }
    }
  }

  client.info(
    `Regenerating sourcemap for ${
      workspaceFolder.name
    } (${workspaceFolder.uri.toString(true)})`
  );
  const args = ["sourcemap", projectFile, "--output", "sourcemap.json"];
  if (config.get<boolean>("includeNonScripts")) {
    args.push("--include-non-scripts");
  }

  spawn(config.get<string>("rojoPath") ?? "rojo", args, {
    cwd: workspaceFolder.uri.fsPath,
  }).catch((err) => {
    let output = `Failed to update sourcemap for ${
      workspaceFolder.name
    } (${workspaceFolder.uri.toString(true)}): `;

    if (
      err.reason.includes("Found argument 'sourcemap' which wasn't expected")
    ) {
      output += `Your Rojo version doesn't have sourcemap support. Upgrade to Rojo v7.1.0+`;
    } else {
      output += err.reason;
    }

    vscode.window.showWarningMessage(output);
  });
};

const parseConfigPath = (path: string) => {
  if (vscode.workspace.workspaceFolders) {
    return resolveUri(vscode.workspace.workspaceFolders[0].uri, path);
  } else {
    return vscode.Uri.file(path);
  }
};

export async function activate(context: vscode.ExtensionContext) {
  console.log("Luau LSP activated");

  const args = ["lsp"];

  const typesConfig = vscode.workspace.getConfiguration("luau-lsp.types");

  // DEPRECATED: Load global type definitions
  const definitionFiles = typesConfig.get<string[]>("definitionFiles");
  if (definitionFiles) {
    for (const definitionPath of definitionFiles) {
      const uri = parseConfigPath(definitionPath);
      if (await exists(uri)) {
        args.push(`--definitions=${uri.fsPath}`);
      } else {
        vscode.window.showWarningMessage(
          `Definitions file at ${definitionPath} does not exist, types will not be provided from this file`
        );
      }
    }
  }

  // Load environments
  const environments: Record<string, string> = {};

  const environmentsConfig =
    typesConfig.get<Record<string, string>>("environments");
  if (environmentsConfig) {
    for (const [name, file] of Object.entries(environmentsConfig)) {
      const uri = parseConfigPath(file);
      if (await exists(uri)) {
        environments[name] = uri.fsPath;
      } else {
        vscode.window.showWarningMessage(
          `Definitions file at ${file} does not exist, types will not be provided for environment ${name}`
        );
      }
    }
  }

  // Load roblox type definitions
  if (typesConfig.get<boolean>("roblox")) {
    await updateApiInfo(context);
    if (!environments["roblox"]) {
      environments["roblox"] = globalTypesUri(context).fsPath;
    }
    args.push(`--docs=${apiDocsUri(context).fsPath}`);
  }

  for (const [name, builtins] of Object.entries(environments)) {
    args.push(`--environment:${name}=${builtins}`);
  }

  // Handle FFlags
  const fflags: FFlags = {};
  const fflagsConfig = vscode.workspace.getConfiguration("luau-lsp.fflags");

  if (!fflagsConfig.get<boolean>("enableByDefault")) {
    args.push("--no-flags-enabled");
  }

  // Sync FFlags with upstream
  if (fflagsConfig.get<boolean>("sync")) {
    try {
      const currentFlags = await getFFlags();
      if (currentFlags) {
        for (const [name, value] of Object.entries(currentFlags)) {
          if (name.startsWith("FFlagLuau")) {
            fflags[name.substring(5)] = value; // Remove the "FFlag" part from the name
          }
        }
      }
    } catch (err) {
      vscode.window.showWarningMessage(
        "Failed to fetch current Luau FFlags: " + err
      );
    }
  }

  // Handle overrides
  const overridenFFlags = fflagsConfig.get<FFlags>("override");
  if (overridenFFlags) {
    for (const [name, value] of Object.entries(overridenFFlags)) {
      fflags[name] = value;
    }
  }

  // Pass FFlags as arguments
  for (const [name, value] of Object.entries(fflags)) {
    args.push(`--flag:${name}=${value}`);
  }

  console.log("Starting server with: ", args);

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
    diagnosticPullOptions: {
      onChange: true,
      onSave: true,
    },
  };

  client = new LanguageClient("luau", "Luau LSP", serverOptions, clientOptions);

  // Register commands
  client.onNotification("$/command", (params) => {
    vscode.commands.executeCommand(params.command, params.data);
  });

  context.subscriptions.push(
    vscode.commands.registerCommand("luau-lsp.updateApi", async () => {
      await downloadApiDefinitions(context);
      vscode.window
        .showInformationMessage(
          "API Types have been updated, reload workspace to take effect.",
          "Reload Workspace"
        )
        .then((command) => {
          if (command === "Reload Workspace") {
            vscode.commands.executeCommand("workbench.action.reloadWindow");
          }
        });
    })
  );

  const updateSourcemapForAllFolders = () => {
    if (vscode.workspace.workspaceFolders) {
      for (const folder of vscode.workspace.workspaceFolders) {
        updateSourceMap(folder);
      }
    }
  };

  context.subscriptions.push(
    vscode.commands.registerCommand(
      "luau-lsp.regenerateSourcemap",
      updateSourcemapForAllFolders
    )
  );

  // Register automatic sourcemap regenerate
  // TODO: maybe we should move this to the server in future
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
      if (e.affectsConfiguration("luau-lsp.sourcemap")) {
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
      } else if (e.affectsConfiguration("luau-lsp.types")) {
        vscode.window
          .showInformationMessage(
            "Luau type definitions have been changed, reload your workspace for this to take effect.",
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

  updateSourcemapForAllFolders();

  console.log("LSP Setup");
  await client.start();
}

export async function deactivate() {
  if (client) {
    await client.stop();
  }
}
