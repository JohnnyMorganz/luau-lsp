import * as vscode from "vscode";
import { Server } from "http";
import express, { ErrorRequestHandler } from "express";
import { format as bytesFormat } from "bytes";
import { fetch } from "undici";
import { spawn } from "child_process";
import { LanguageClient } from "vscode-languageclient/node";
import { AddArgCallback, PlatformContext } from "./extension";

import * as utils from "./utils";

let pluginServer: Server | undefined = undefined;

const API_DOCS = "https://luau-lsp.pages.dev/api-docs/en-us.json";
const STUDIO_PLUGIN_URL =
  "https://www.roblox.com/library/10913122509/Luau-Language-Server-Companion";

const setupStudioPlugin = async (client: LanguageClient | undefined) => {
  // Enable the plugin server
  await vscode.workspace
    .getConfiguration("luau-lsp.plugin")
    .update("enabled", true);
  startPluginServer(client);
  // Open the studio plugin in the browser for the user to install
  vscode.env.openExternal(vscode.Uri.parse(STUDIO_PLUGIN_URL));
};

const globalTypesEndpointForSecurityLevel = (securityLevel: string) => {
  return `https://luau-lsp.pages.dev/type-definitions/globalTypes.${securityLevel}.d.luau`;
};

const globalTypesUri = (
  context: vscode.ExtensionContext,
  securityLevel: string,
  mode: "Prod" | "Debug",
) => {
  if (mode === "Prod") {
    return vscode.Uri.joinPath(
      context.globalStorageUri,
      `globalTypes.${securityLevel}.d.luau`,
    );
  } else {
    return vscode.Uri.joinPath(
      context.extensionUri,
      "..",
      "..",
      `scripts/globalTypes.${securityLevel}.d.luau`,
    );
  }
};

const apiDocsUri = (context: vscode.ExtensionContext) => {
  return vscode.Uri.joinPath(context.globalStorageUri, "api-docs.json");
};

const getRojoProjectFile = async (
  workspaceFolder: vscode.WorkspaceFolder,
  config: vscode.WorkspaceConfiguration,
  client: LanguageClient | undefined,
) => {
  let projectFile =
    config.get<string>("rojoProjectFile") ?? "default.project.json";
  const projectFileUri = utils.resolveUri(workspaceFolder.uri, projectFile);

  if (await utils.exists(projectFileUri)) {
    return projectFile;
  }

  // Search if there is a *.project.json file present in this workspace.
  const foundProjectFiles = await vscode.workspace.findFiles(
    new vscode.RelativePattern(workspaceFolder.uri, "*.project.json"),
  );

  if (foundProjectFiles.length === 0) {
    // If the plugin is not enabled, provide a one-click setup button
    let options: string[] = [];
    if (!vscode.workspace.getConfiguration("luau-lsp.plugin").get<boolean>("enabled")) {
      options.push("Setup Plugin");
    }
    options.push("Configure Settings");
    vscode.window
      .showWarningMessage(
          `Unable to find project file ${projectFile} for Rojo sourcemap generation. Configure a file in settings, or use the Studio Plugin for DataModel info instead`,
        ...options
      )
      .then((value) => {
        if (value === "Setup Plugin") {
          setupStudioPlugin(client);
        } else if (value === "Configure Settings") {
          vscode.commands.executeCommand(
            "workbench.action.openWorkspaceSettings",
            "luau-lsp.sourcemap",
          );
        }
      });
    return undefined;
  } else if (foundProjectFiles.length === 1) {
    const fileName = utils.basenameUri(foundProjectFiles[0]);
    const option = await vscode.window.showWarningMessage(
      `Unable to find project file ${projectFile} for Rojo sourcemap generation. We found ${fileName} available`,
      `Set project file to ${fileName}`,
      "Cancel",
    );

    if (option === `Set project file to ${fileName}`) {
      config.update("rojoProjectFile", fileName);
      return fileName;
    } else {
      return undefined;
    }
  } else {
    const option = await vscode.window.showWarningMessage(
      `Unable to find project file ${projectFile} for Rojo sourcemap generation. We found ${foundProjectFiles.length} files available`,
      "Select project file",
      "Cancel",
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

const sourcemapDisposables: Map<
  vscode.WorkspaceFolder,
  Array<vscode.Disposable>
> = new Map();

const addSourcemapDisposable = (
  workspaceFolder: vscode.WorkspaceFolder,
  disposable: vscode.Disposable,
) => {
  if (!sourcemapDisposables.get(workspaceFolder)) {
    sourcemapDisposables.set(workspaceFolder, []);
  }
  sourcemapDisposables.get(workspaceFolder)!.push(disposable);
};

const cleanupSourcemapDisposables = async (
  workspaceFolder: vscode.WorkspaceFolder,
) => {
  const disposables = sourcemapDisposables.get(workspaceFolder);
  if (disposables) {
    for (const disposable of disposables) {
      disposable.dispose();
    }
  }
  sourcemapDisposables.delete(workspaceFolder);
};

const startSourcemapGeneration = async (
  client: LanguageClient | undefined,
  workspaceFolder: vscode.WorkspaceFolder,
) => {
  cleanupSourcemapDisposables(workspaceFolder);

  const config = vscode.workspace.getConfiguration(
    "luau-lsp.sourcemap",
    workspaceFolder,
  );

  if (!config.get<boolean>("enabled") || !config.get<boolean>("autogenerate")) {
    return;
  }

  const customGeneratorCommand = config.get<string>("generatorCommand");
  const useVSCodeWatcher = config.get<boolean>("useVSCodeWatcher") ?? false;

  const loggingFunc = client ? client.info.bind(client) : console.log;
  loggingFunc(
    `Starting sourcemap generation for ${
      workspaceFolder.name
    } (${workspaceFolder.uri.toString(true)})`,
  );

  const cwd = workspaceFolder.uri.fsPath;

  const spawnChildProcess = async () => {
    loggingFunc(
      `Spawning sourcemap generator for ${
        workspaceFolder.name
      } (${workspaceFolder.uri.toString(true)})`,
    );

    let childProcess;

    if (customGeneratorCommand && customGeneratorCommand.trim() !== "") {
      // TODO: should we support shell execution here?
      // It allows us to delegate to the shell for argument parsing
      // but it causes issues when VSCode shuts down, leaving a zombie process
      childProcess = spawn(customGeneratorCommand, {
        cwd,
        shell: true,
      });
    } else {
      // Check if the project file exists
      const projectFile = await getRojoProjectFile(
        workspaceFolder,
        config,
        client,
      );
      if (!projectFile) {
        return;
      }
      const rojoPath = config.get<string>("rojoPath") ?? "rojo";
      const sourcemapFileName =
        config.get<string>("sourcemapFile") ?? "sourcemap.json";
      const args = ["sourcemap", projectFile, "--output", sourcemapFileName];

      if (config.get<boolean>("includeNonScripts")) {
        args.push("--include-non-scripts");
      }

      if (!useVSCodeWatcher) {
        args.push("--watch");
      }

      childProcess = spawn(rojoPath, args, { cwd });
    }

    let stderr = "";
    childProcess.stderr.on("data", (data) => {
      stderr += data;
    });

    childProcess.on("error", (err) => {
      stderr += err.message;
    });

    childProcess.on("close", (code, signal) => {
      if (childProcess.killed) {
        return;
      }
      if (code !== 0) {
        let output = `Failed to update sourcemap for ${workspaceFolder.name}: `;
        let options = ["Retry"];

        if (customGeneratorCommand) {
          output += stderr;
          if (stderr === "") {
            output += "<no output>";
          }
          options.push("Configure Settings");
        } else {
          if (
            stderr.includes("Found argument 'sourcemap' which wasn't expected")
          ) {
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
              "Rojo not found. Configure your Rojo path in settings, or use the Studio Plugin for DataModel info instead";
            if (!vscode.workspace.getConfiguration("luau-lsp.plugin").get<boolean>("enabled")) {
              options.push("Setup Plugin");
            }
            options.push("Configure Settings");
          } else {
            output += stderr;
          }
        }

        vscode.window.showWarningMessage(output, ...options).then((value) => {
          if (value === "Retry") {
            startSourcemapGeneration(client, workspaceFolder);
          } else if (value === "Setup Plugin") {
            setupStudioPlugin(client);
          } else if (value === "Configure Settings") {
            vscode.commands.executeCommand(
              "workbench.action.openWorkspaceSettings",
              "luau-lsp.sourcemap",
            );
          }
        });
      }
    });

    return childProcess;
  };

  if (useVSCodeWatcher) {
    spawnChildProcess();

    const watcher = vscode.workspace.createFileSystemWatcher(
      new vscode.RelativePattern(workspaceFolder, "**/*.{lua,luau}"),
      /* ignoreCreateEvents = */ false,
      /* ignoreChangeEvents = */ true,
      /* ignoreDeleteEvents = */ false,
    );

    let debounceTimer: NodeJS.Timeout;
    watcher.onDidCreate(() => {
      clearTimeout(debounceTimer);
      debounceTimer = setTimeout(spawnChildProcess, 1000);
    });
    watcher.onDidDelete(() => {
      clearTimeout(debounceTimer);
      debounceTimer = setTimeout(spawnChildProcess, 1000);
    });

    addSourcemapDisposable(workspaceFolder, watcher);
  } else {
    const childProcess = await spawnChildProcess();
    if (childProcess) {
      childProcess.on("close", (code) => {
        cleanupSourcemapDisposables(workspaceFolder);

        if (code === 0) {
          vscode.window
            .showWarningMessage(
              "Sourcemap generator ended. No further updates will be tracked. If the generator does not support file watching, enable luau-lsp.sourcemap.useVSCodeWatcher",
              "Restart",
              "Configure Settings",
            )
            .then((value) => {
              if (value === "Restart") {
                startSourcemapGeneration(client, workspaceFolder);
              } else if (value === "Configure Settings") {
                vscode.commands.executeCommand(
                  "workbench.action.openWorkspaceSettings",
                  "luau-lsp.sourcemap",
                );
              }
            });
        }
      });
      addSourcemapDisposable(
        workspaceFolder,
        new vscode.Disposable(() => {
          if (childProcess.killed) {
            return;
          }
          childProcess.kill();
        }),
      );
    }
  }
};

const startPluginServer = async (client: LanguageClient | undefined) => {
  if (pluginServer) {
    return;
  }

  const app = express();
  app.use(
    express.json({
      limit: vscode.workspace
        .getConfiguration("luau-lsp.plugin")
        .get("maximumRequestBodySize", "3mb"),
    }),
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

  const errorHandler: ErrorRequestHandler = (err, req, res, next) => {
    if (res.headersSent) {
      return next(err);
    }

    if (err && err.type === "entity.too.large") {
      res
        .status(413)
        .send(
          `Result is too large. Limit: ${bytesFormat(err.limit)}, Received: ${bytesFormat(err.received)}.\n` +
            `Increase your available limits by updating the 'luau-lsp.plugin.maximumRequestBodySize' property in VSCode, or by reducing the include list in the Studio Plugin settings`,
        );
    }
  };

  app.use(errorHandler);

  const port = vscode.workspace.getConfiguration("luau-lsp.plugin").get("port");
  pluginServer = app
    .listen(port, () => {
      vscode.window.showInformationMessage(
        `Luau Language Server Studio Plugin is now listening on port ${port}`,
      );
    })
    .on("error", (err) => {
      if ((err as any).code === "EADDRINUSE") {
        vscode.window
          .showErrorMessage(
            `Failed to start Luau Language Server Studio Plugin on port ${port}: Port already in use. Check there are no other servers running on this port, or change the port in settings`,
            "Reconnect",
            "Change Port Configuration",
          )
          .then((value) => {
            if (value === "Reconnect") {
              stopPluginServer(true);
              startPluginServer(client);
            } else if (value === "Change Port Configuration") {
              vscode.commands.executeCommand(
                "workbench.action.openWorkspaceSettings",
                "luau-lsp.plugin.port",
              );
            }
          });
      } else {
        vscode.window.showErrorMessage(
          `Failed to start Luau Language Server Studio Plugin on port ${port}: ${err}`,
        );
      }
    });
};

const stopPluginServer = async (isDeactivating = false) => {
  if (pluginServer) {
    pluginServer.close();
    pluginServer = undefined;

    if (!isDeactivating) {
      vscode.window.showInformationMessage(
        `Luau Language Server Studio Plugin has disconnected`,
      );
    }
  }
};

export const onActivate = async (
  platformContext: PlatformContext,
  context: vscode.ExtensionContext,
) => {
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
      startSourcemapGenerationForAllFolders,
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("luau-lsp.setupStudioPlugin", () =>
      setupStudioPlugin(platformContext.client),
    ),
  );

  context.subscriptions.push(
    vscode.workspace.onDidChangeConfiguration((e) => {
      if (e.affectsConfiguration("luau-lsp.sourcemap")) {
        if (vscode.workspace.workspaceFolders) {
          for (const folder of vscode.workspace.workspaceFolders) {
            const config = vscode.workspace.getConfiguration(
              "luau-lsp.sourcemap",
              folder,
            );

            if (
              !config.get<boolean>("enabled") ||
              !config.get<boolean>("autogenerate")
            ) {
              cleanupSourcemapDisposables(folder);
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
    }),
  );

  startSourcemapGenerationForAllFolders();
};

export const preLanguageServerStart = async (
  context: vscode.ExtensionContext,
) => {
  // Load roblox type definitions
  const typesConfig = vscode.workspace.getConfiguration("luau-lsp.types");
  const platformConfig = vscode.workspace.getConfiguration("luau-lsp.platform");

  // TODO: Cleanup when deprecated luau-lsp.types.roblox is deleted
  // We need to respect the new setting as well as the old setting. We check for "&&" since they are on by default
  if (
    platformConfig.get<string>("type") === "roblox" &&
    typesConfig.get<boolean>("roblox")
  ) {
    const securityLevel =
      typesConfig.get<string>("robloxSecurityLevel") ?? "PluginSecurity";

    return {
      definitions: {
        ["@roblox"]: {
          url: globalTypesEndpointForSecurityLevel(securityLevel),
          outputUri: globalTypesUri(context, securityLevel, "Prod"),
        },
      },
      documentation: [{ url: API_DOCS, outputUri: apiDocsUri(context) }],
    };
  } else {
    return {
      definition: undefined,
      documentation: undefined,
    };
  }
};

export const postLanguageServerStart = async (
  platformContext: PlatformContext,
  _: vscode.ExtensionContext,
) => {
  if (
    vscode.workspace.getConfiguration("luau-lsp.plugin").get<boolean>("enabled")
  ) {
    startPluginServer(platformContext.client);
  }
};

export const onDeactivate = () => {
  return [
    ...Array.from(sourcemapDisposables.keys()).map((workspace) =>
      cleanupSourcemapDisposables(workspace),
    ),
    stopPluginServer(true),
  ];
};
