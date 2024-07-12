import * as vscode from "vscode";
import * as os from "os";
import fetch from "node-fetch";
import {
  Executable,
  ServerOptions,
  LanguageClient,
  LanguageClientOptions,
} from "vscode-languageclient/node";

import {
  registerComputeBytecode,
  registerComputeCompilerRemarks,
} from "./bytecode";

import * as roblox from "./roblox";
import * as utils from "./utils";

export type PlatformContext = { client: LanguageClient | undefined };
export type AddArgCallback = (
  argument: string,
  mode?: "All" | "Prod" | "Debug"
) => void;

let client: LanguageClient | undefined = undefined;
let platformContext: PlatformContext = { client: undefined };
const clientDisposables: vscode.Disposable[] = [];

const CURRENT_FFLAGS =
  "https://clientsettingscdn.roblox.com/v1/settings/application?applicationName=PCDesktopClient";

type FFlags = Record<string, string>;
type FFlagsEndpoint = { applicationSettings: FFlags };

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

const startLanguageServer = async (context: vscode.ExtensionContext) => {
  for (const disposable of clientDisposables) {
    disposable.dispose();
  }
  clientDisposables.splice(0, clientDisposables.length); // empty the list
  if (client) {
    await client.stop();
  }

  console.log("Starting Luau Language Server");

  const args = ["lsp"];
  const debugArgs = ["lsp"];

  const addArg = (argument: string, mode: "All" | "Prod" | "Debug" = "All") => {
    if (mode === "All" || mode === "Prod") {
      args.push(argument);
    }
    if (mode === "All" || mode === "Debug") {
      debugArgs.push(argument);
    }
  };

  await roblox.preLanguageServerStart(platformContext, context, addArg);

  const typesConfig = vscode.workspace.getConfiguration("luau-lsp.types");

  // Load extra type definitions
  let definitionFiles = typesConfig.get<
    { [packageName: string]: string } | string[]
  >("definitionFiles");
  if (definitionFiles) {
    if (Array.isArray(definitionFiles)) {
      // Convert to a map structure
      definitionFiles = Object.fromEntries(
        definitionFiles.map((path, index) => ["roblox" + index, path])
      );
    }

    for (const definitionPath of definitionFiles) {
      let uri;
      if (vscode.workspace.workspaceFolders) {
        uri = utils.resolveUri(
          vscode.workspace.workspaceFolders[0].uri,
          definitionPath
        );
      } else {
        uri = vscode.Uri.file(definitionPath);
      }
      if (await utils.exists(uri)) {
        addArg(`--definitions=${uri.fsPath}`);
      } else {
        vscode.window.showWarningMessage(
          `Definitions file at ${definitionPath} does not exist, types will not be provided from this file`
        );
      }
    }
  }

  // Load extra documentation files
  const documentationFiles = typesConfig.get<string[]>("documentationFiles");
  if (documentationFiles) {
    for (const documentationPath of documentationFiles) {
      let uri;
      if (vscode.workspace.workspaceFolders) {
        uri = utils.resolveUri(
          vscode.workspace.workspaceFolders[0].uri,
          documentationPath
        );
      } else {
        uri = vscode.Uri.file(documentationPath);
      }
      if (await utils.exists(uri)) {
        addArg(`--docs=${uri.fsPath}`);
      } else {
        vscode.window.showWarningMessage(
          `Documentations file at ${documentationPath} does not exist`
        );
      }
    }
  }

  // Handle FFlags
  const fflags: FFlags = {};
  const fflagsConfig = vscode.workspace.getConfiguration("luau-lsp.fflags");

  if (!fflagsConfig.get<boolean>("enableByDefault")) {
    addArg("--no-flags-enabled");
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
      // Validate that the name and value is valid
      if (name.length > 0 && value.length > 0) {
        fflags[name] = value;
      }
    }
  }

  const run: Executable = {
    command: vscode.Uri.joinPath(
      context.extensionUri,
      "bin",
      os.platform() === "win32" ? "server.exe" : "server"
    ).fsPath,
    args,
  };

  // If debugging, run the locally build extension, with local type definitions file
  const debug: Executable = {
    command: vscode.Uri.joinPath(
      context.extensionUri,
      "..",
      "..",
      process.env["SERVER_PATH"] ?? "unknown.exe"
    ).fsPath,
    args: debugArgs,
  };

  const serverOptions: ServerOptions = { run, debug };

  const config = {
    default: vscode.workspace.getConfiguration("luau-lsp"),
    ...Object.fromEntries(
      vscode.workspace.workspaceFolders?.map((folder) => [
        folder.uri,
        vscode.workspace.getConfiguration("luau-lsp", folder),
      ]) ?? []
    ),
  };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [
      { language: "lua", scheme: "file" },
      { language: "luau", scheme: "file" },
      { language: "lua", scheme: "untitled" },
      { language: "luau", scheme: "untitled" },
    ],
    diagnosticPullOptions: {
      onChange: true,
      onSave: true,
    },
    initializationOptions: {
      fflags,
      config,
    },
  };

  client = new LanguageClient(
    "luau",
    "Luau Language Server",
    serverOptions,
    clientOptions
  );

  platformContext.client = client;

  // Register commands
  client.onNotification("$/command", (params) => {
    vscode.commands.executeCommand(params.command, params.data);
  });

  clientDisposables.push(...registerComputeBytecode(context, client));
  clientDisposables.push(...registerComputeCompilerRemarks(context, client));

  console.log("LSP Setup");
  await client.start();
};

export async function activate(context: vscode.ExtensionContext) {
  console.log("Luau LSP activated");

  await roblox.onActivate(platformContext, context);

  context.subscriptions.push(
    vscode.commands.registerCommand("luau-lsp.reloadServer", async () => {
      vscode.window.showInformationMessage("Reloading Language Server");
      await startLanguageServer(context);
    })
  );

  context.subscriptions.push(
    vscode.workspace.onDidChangeConfiguration((e) => {
      if (e.affectsConfiguration("luau-lsp.fflags")) {
        vscode.window
          .showInformationMessage(
            "Luau FFlags have been changed, reload server for this to take effect.",
            "Reload Language Server"
          )
          .then((command) => {
            if (command === "Reload Language Server") {
              vscode.commands.executeCommand("luau-lsp.reloadServer");
            }
          });
      } else if (
        e.affectsConfiguration("luau-lsp.types") ||
        e.affectsConfiguration("luau-lsp.platform.type")
      ) {
        vscode.window
          .showInformationMessage(
            "Luau type definitions have been changed, reload server for this to take effect.",
            "Reload Language Server"
          )
          .then((command) => {
            if (command === "Reload Language Server") {
              vscode.commands.executeCommand("luau-lsp.reloadServer");
            }
          });
      }
    })
  );

  await startLanguageServer(context);

  await roblox.postLanguageServerStart(platformContext, context);
}

export async function deactivate() {
  return Promise.allSettled([
    ...roblox.onDeactivate(),
    client?.stop(),
    clientDisposables.map((disposable) => disposable.dispose()),
  ]);
}
