import * as vscode from "vscode";
import { Server } from "http";
import express from "express";
import { spawn, ChildProcess } from "child_process";
import { LanguageClient } from "vscode-languageclient/node";
import { AddArgCallback, PlatformContext } from "./extension";

import * as utils from "./utils";

let pluginServer: Server | undefined = undefined;

const CURRENT_VERSION_TXT =
  "https://raw.githubusercontent.com/CloneTrooper1019/Roblox-Client-Tracker/roblox/version.txt";
const API_DOCS =
  "https://raw.githubusercontent.com/MaximumADHD/Roblox-Client-Tracker/roblox/api-docs/en-us.json";

const SECURITY_LEVELS = [
  "None",
  "LocalUserSecurity",
  "PluginSecurity",
  "RobloxScriptSecurity",
];

const globalTypesEndpointForSecurityLevel = (securityLevel: string) => {
  return `https://raw.githubusercontent.com/JohnnyMorganz/luau-lsp/main/scripts/globalTypes.${securityLevel}.d.luau`;
};

const globalTypesUri = (
  context: vscode.ExtensionContext,
  securityLevel: string,
  mode: "Prod" | "Debug"
) => {
  if (mode === "Prod") {
    return vscode.Uri.joinPath(
      context.globalStorageUri,
      `globalTypes.${securityLevel}.d.luau`
    );
  } else {
    return vscode.Uri.joinPath(
      context.extensionUri,
      "..",
      "..",
      `scripts/globalTypes.${securityLevel}.d.luau`
    );
  }
};

const apiDocsUri = (context: vscode.ExtensionContext) => {
  return vscode.Uri.joinPath(context.globalStorageUri, "api-docs.json");
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
          ...SECURITY_LEVELS.map((level) =>
            fetch(globalTypesEndpointForSecurityLevel(level))
              .then((r) => r.arrayBuffer())
              .then((data) =>
                vscode.workspace.fs.writeFile(
                  globalTypesUri(context, level, "Prod"),
                  new Uint8Array(data)
                )
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
      (
        await Promise.all(
          SECURITY_LEVELS.map(
            async (level) =>
              await utils.exists(globalTypesUri(context, level, "Prod"))
          )
        )
      ).some((doesExist) => !doesExist) ||
      !(await utils.exists(apiDocsUri(context)));

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

const getRojoProjectFile = async (
  workspaceFolder: vscode.WorkspaceFolder,
  config: vscode.WorkspaceConfiguration
) => {
  let projectFile =
    config.get<string>("rojoProjectFile") ?? "default.project.json";
  const projectFileUri = utils.resolveUri(workspaceFolder.uri, projectFile);

  if (await utils.exists(projectFileUri)) {
    return projectFile;
  }

  // Search if there is a *.project.json file present in this workspace.
  const foundProjectFiles = await vscode.workspace.findFiles(
    new vscode.RelativePattern(workspaceFolder.uri, "*.project.json")
  );

  if (foundProjectFiles.length === 0) {
    vscode.window.showWarningMessage(
      `Unable to find project file ${projectFile}. Please configure a file in settings`
    );
    return undefined;
  } else if (foundProjectFiles.length === 1) {
    const fileName = utils.basenameUri(foundProjectFiles[0]);
    const option = await vscode.window.showWarningMessage(
      `Unable to find project file ${projectFile}. We found ${fileName} available`,
      `Set project file to ${fileName}`,
      "Cancel"
    );

    if (option === `Set project file to ${fileName}`) {
      config.update("rojoProjectFile", fileName);
      return fileName;
    } else {
      return undefined;
    }
  } else {
    const option = await vscode.window.showWarningMessage(
      `Unable to find project file ${projectFile}. We found ${foundProjectFiles.length} files available`,
      "Select project file",
      "Cancel"
    );
    if (option === "Select project file") {
      const files = foundProjectFiles.map((file) => utils.basenameUri(file));
      const selectedFile = await vscode.window.showQuickPick(files);
      if (selectedFile) {
        config.update("rojoProjectFile", selectedFile);
        selectedFile;
      } else {
        return undefined;
      }
    } else {
      return undefined;
    }
  }

  return undefined;
};

const sourcemapGeneratorProcesses: Map<vscode.WorkspaceFolder, ChildProcess> =
  new Map();

const stopSourcemapGeneration = async (
  workspaceFolder: vscode.WorkspaceFolder
) => {
  const process = sourcemapGeneratorProcesses.get(workspaceFolder);
  if (process) {
    process.kill();
  }
  sourcemapGeneratorProcesses.delete(workspaceFolder);
};

const startSourcemapGeneration = async (
  client: LanguageClient | undefined,
  workspaceFolder: vscode.WorkspaceFolder
) => {
  stopSourcemapGeneration(workspaceFolder);
  const config = vscode.workspace.getConfiguration(
    "luau-lsp.sourcemap",
    workspaceFolder
  );
  if (!config.get<boolean>("enabled") || !config.get<boolean>("autogenerate")) {
    return;
  }

  // Check if the project file exists
  const projectFile = await getRojoProjectFile(workspaceFolder, config);
  if (!projectFile) {
    return;
  }

  const loggingFunc = client ? client.info.bind(client) : console.log;
  loggingFunc(
    `Starting sourcemap generation for ${
      workspaceFolder.name
    } (${workspaceFolder.uri.toString(true)})`
  );

  const workspacePath = workspaceFolder.uri.fsPath;
  const rojoPath = config.get<string>("rojoPath") ?? "rojo";
  const args = [
    "sourcemap",
    projectFile,
    "--watch",
    "--output",
    "sourcemap.json",
  ];
  if (config.get<boolean>("includeNonScripts")) {
    args.push("--include-non-scripts");
  }

  const childProcess = spawn(rojoPath, args, {
    cwd: workspacePath,
  });

  sourcemapGeneratorProcesses.set(workspaceFolder, childProcess);

  let stderr = "";
  childProcess.stderr.on("data", (data) => {
    stderr += data;
  });

  childProcess.on("close", (code, signal) => {
    sourcemapGeneratorProcesses.delete(workspaceFolder);
    if (childProcess.killed) {
      return;
    }
    if (code !== 0) {
      let output = `Failed to update sourcemap for ${workspaceFolder.name}: `;
      let options = ["Retry"];

      if (stderr.includes("Found argument 'sourcemap' which wasn't expected")) {
        output +=
          "Your Rojo version doesn't have sourcemap support. Upgrade to Rojo v7.3.0+";
      } else if (
        stderr.includes("Found argument '--watch' which wasn't expected")
      ) {
        output +=
          "Your Rojo version doesn't have sourcemap watching support. Upgrade to Rojo v7.3.0+";
      } else if (
        stderr.includes("is not recognized") ||
        stderr.includes("ENOENT")
      ) {
        output +=
          "Rojo not found. Please install Rojo to your PATH or disable sourcemap autogeneration";
        options.push("Configure Settings");
      } else {
        output += stderr;
      }

      vscode.window.showWarningMessage(output, ...options).then((value) => {
        if (value === "Retry") {
          startSourcemapGeneration(client, workspaceFolder);
        } else if (value === "Configure Settings") {
          vscode.commands.executeCommand(
            "workbench.action.openSettings",
            "luau-lsp.sourcemap"
          );
        }
      });
    }
  });
};

const startPluginServer = async (client: LanguageClient | undefined) => {
  if (pluginServer) {
    return;
  }

  const app = express();
  app.use(
    express.json({
      limit: "3mb",
    })
  );

  app.post("/full", (req, res) => {
    if (!client) {
      return res.sendStatus(500);
    }

    if (req.body.tree) {
      client.sendNotification("$/plugin/full", req.body.tree);
      res.sendStatus(200);
    } else {
      res.sendStatus(400);
    }
  });

  app.post("/clear", (_req, res) => {
    if (!client) {
      return res.sendStatus(500);
    }

    client.sendNotification("$/plugin/clear");
    res.sendStatus(200);
  });

  const port = vscode.workspace.getConfiguration("luau-lsp.plugin").get("port");
  pluginServer = app.listen(port);

  vscode.window.showInformationMessage(
    `Luau Language Server Studio Plugin is now listening on port ${port}`
  );
};

const stopPluginServer = async (isDeactivating = false) => {
  if (pluginServer) {
    pluginServer.close();
    pluginServer = undefined;

    if (!isDeactivating) {
      vscode.window.showInformationMessage(
        `Luau Language Server Studio Plugin has disconnected`
      );
    }
  }
};

export const onActivate = async (
  platformContext: PlatformContext,
  context: vscode.ExtensionContext
) => {
  context.subscriptions.push(
    vscode.commands.registerCommand("luau-lsp.updateApi", async () => {
      await downloadApiDefinitions(context);
      vscode.window
        .showInformationMessage(
          "API Types have been updated, reload server to take effect.",
          "Reload Language Server"
        )
        .then((command) => {
          if (command === "Reload Language Server") {
            vscode.commands.executeCommand("luau-lsp.reloadServer");
          }
        });
    })
  );

  const startSourcemapGenerationForAllFolders = () => {
    if (vscode.workspace.workspaceFolders) {
      for (const folder of vscode.workspace.workspaceFolders) {
        startSourcemapGeneration(platformContext.client, folder);
      }
    }
  };

  context.subscriptions.push(
    vscode.commands.registerCommand(
      "luau-lsp.regenerateSourcemap",
      startSourcemapGenerationForAllFolders
    )
  );

  context.subscriptions.push(
    vscode.workspace.onDidChangeConfiguration((e) => {
      if (e.affectsConfiguration("luau-lsp.sourcemap")) {
        if (vscode.workspace.workspaceFolders) {
          for (const folder of vscode.workspace.workspaceFolders) {
            const config = vscode.workspace.getConfiguration(
              "luau-lsp.sourcemap",
              folder
            );
            if (
              !config.get<boolean>("enabled") ||
              !config.get<boolean>("autogenerate")
            ) {
              stopSourcemapGeneration(folder);
            } else {
              startSourcemapGeneration(platformContext.client, folder);
            }
          }
        }
      } else if (e.affectsConfiguration("luau-lsp.plugin")) {
        if (
          vscode.workspace
            .getConfiguration("luau-lsp.plugin")
            .get<boolean>("enabled")
        ) {
          stopPluginServer(true);
          startPluginServer(platformContext.client);
        } else {
          stopPluginServer();
        }
      }
    })
  );

  startSourcemapGenerationForAllFolders();
};

export const preLanguageServerStart = async (
  _: PlatformContext,
  context: vscode.ExtensionContext,
  addArg: AddArgCallback
) => {
  // Load roblox type definitions
  const typesConfig = vscode.workspace.getConfiguration("luau-lsp.types");
  if (typesConfig.get<boolean>("roblox")) {
    const securityLevel =
      typesConfig.get<string>("robloxSecurityLevel") ?? "PluginSecurity";
    await updateApiInfo(context);
    addArg(
      `--definitions=${globalTypesUri(context, securityLevel, "Prod").fsPath}`,
      "Prod"
    );
    addArg(
      `--definitions=${globalTypesUri(context, securityLevel, "Debug").fsPath}`,
      "Debug"
    );
    addArg(`--docs=${apiDocsUri(context).fsPath}`);
  }
};

export const postLanguageServerStart = async (
  platformContext: PlatformContext,
  _: vscode.ExtensionContext
) => {
  if (
    vscode.workspace.getConfiguration("luau-lsp.plugin").get<boolean>("enabled")
  ) {
    startPluginServer(platformContext.client);
  }
};

export const onDeactivate = () => {
  return [
    ...Array.from(sourcemapGeneratorProcesses.keys()).map((workspace) =>
      stopSourcemapGeneration(workspace)
    ),
    stopPluginServer(true),
  ];
};
