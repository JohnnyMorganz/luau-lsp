import * as vscode from "vscode";
import * as os from "os";
import { fetch } from "undici";
import {
  CloseAction,
  CloseHandlerResult,
  ErrorAction,
  ErrorHandler,
  ErrorHandlerResult,
  Executable,
  LanguageClient,
  LanguageClientOptions,
  Message,
  ServerOptions,
  TransportKind,
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
  mode?: "All" | "Prod" | "Debug",
) => void;

let client: LanguageClient | undefined = undefined;
let platformContext: PlatformContext = { client: undefined };
const clientDisposables: vscode.Disposable[] = [];

const CURRENT_FFLAGS =
  "https://clientsettingscdn.roblox.com/v1/settings/application?applicationName=PCStudioApp";
const FFLAG_KINDS = ["FFlag", "FInt", "DFFlag", "DFInt"];

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
        .then((r) => r.applicationSettings),
  );
};

const isAlphanumericUnderscore = (str: string) => {
  return /^[a-zA-Z0-9_]+$/.test(str);
};

const isCrashReportingEnabled = () => {
  const config = vscode.workspace.getConfiguration("luau-lsp.server");
  return config.get("crashReporting.enabled", false);
};

const DO_NOT_SHOW_CRASH_REPORTING_SUGGESTION_KEY =
  "doNotShowCrashReportingSuggestion";

const shouldShowEnableCrashReportsMessage = (
  context: vscode.ExtensionContext,
) => {
  return !context.globalState.get<boolean>(
    DO_NOT_SHOW_CRASH_REPORTING_SUGGESTION_KEY,
  );
};

class ClientErrorHandler implements ErrorHandler {
  private readonly restarts: number[];
  private recommendedCrashReporting: boolean = false;

  constructor(
    private context: vscode.ExtensionContext,
    private maxRestartCount: number,
  ) {
    this.restarts = [];
  }

  public error(
    _error: Error,
    _message: Message,
    count: number,
  ): ErrorHandlerResult {
    if (count && count <= 3) {
      return { action: ErrorAction.Continue };
    }
    return { action: ErrorAction.Shutdown };
  }

  public closed(): CloseHandlerResult {
    if (
      !this.recommendedCrashReporting &&
      !isCrashReportingEnabled() &&
      shouldShowEnableCrashReportsMessage(this.context)
    ) {
      this.recommendedCrashReporting = true;
      vscode.window
        .showInformationMessage(
          "The Luau Language server exited unexpected. Would you like to enable crash reporting?",
          "Enable",
          "Not now",
          "Do not show again",
        )
        .then((value) => {
          if (value === "Enable") {
            vscode.workspace
              .getConfiguration("luau-lsp.server")
              .update(
                "crashReporting.enabled",
                true,
                vscode.ConfigurationTarget.Global,
              );
          } else if (value === "Do not show again") {
            this.context.globalState.update(
              DO_NOT_SHOW_CRASH_REPORTING_SUGGESTION_KEY,
              true,
            );
          }
        });
    }

    this.restarts.push(Date.now());
    if (this.restarts.length <= this.maxRestartCount) {
      return { action: CloseAction.Restart };
    } else {
      const diff = this.restarts[this.restarts.length - 1] - this.restarts[0];
      if (diff <= 3 * 60 * 1000) {
        return {
          action: CloseAction.DoNotRestart,
          message: `The Luau Language server crashed ${this.maxRestartCount + 1} times in the last 3 minutes. The server will not be restarted. See the output for more information.`,
        };
      } else {
        this.restarts.shift();
        return { action: CloseAction.Restart };
      }
    }
  }
}

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
  const definitionFiles = typesConfig.get<string[]>("definitionFiles");
  if (definitionFiles) {
    for (let definitionPath of definitionFiles) {
      definitionPath = utils.resolvePath(definitionPath);
      let uri;
      if (vscode.workspace.workspaceFolders) {
        uri = utils.resolveUri(
          vscode.workspace.workspaceFolders[0].uri,
          definitionPath,
        );
      } else {
        uri = vscode.Uri.file(definitionPath);
      }
      if (await utils.exists(uri)) {
        addArg(`--definitions=${uri.fsPath}`);
      } else {
        vscode.window.showWarningMessage(
          `Definitions file at ${definitionPath} does not exist, types will not be provided from this file`,
        );
      }
    }
  }

  // Load extra documentation files
  const documentationFiles = typesConfig.get<string[]>("documentationFiles");
  if (documentationFiles) {
    for (let documentationPath of documentationFiles) {
      documentationPath = utils.resolvePath(documentationPath);
      let uri;
      if (vscode.workspace.workspaceFolders) {
        uri = utils.resolveUri(
          vscode.workspace.workspaceFolders[0].uri,
          documentationPath,
        );
      } else {
        uri = vscode.Uri.file(documentationPath);
      }
      if (await utils.exists(uri)) {
        addArg(`--docs=${uri.fsPath}`);
      } else {
        vscode.window.showWarningMessage(
          `Documentations file at ${documentationPath} does not exist`,
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
          for (const kind of FFLAG_KINDS) {
            if (name.startsWith(`${kind}Luau`)) {
              // Remove the "FFlag" part from the name
              fflags[name.substring(kind.length)] = value;
            }
          }
        }
      }
    } catch (err) {
      vscode.window.showWarningMessage(
        "Failed to fetch current Luau FFlags: " + err,
      );
    }
  }

  // Enable new solver
  if (fflagsConfig.get<boolean>("enableNewSolver")) {
    fflags["LuauSolverV2"] = "true";
  }

  if (
    vscode.workspace
      .getConfiguration("luau-lsp.completion")
      .get<boolean>("enableFragmentAutocomplete")
  ) {
    fflags["LuauBetterScopeSelection"] = "true";
    fflags["LuauBlockDiffFragmentSelection"] = "true";
    fflags["LuauFragmentAcMemoryLeak"] = "true";
    fflags["LuauGlobalVariableModuleIsolation"] = "true";
    fflags["LuauFragmentAutocompleteIfRecommendations"] = "true";
    fflags["LuauPopulateRefinedTypesInFragmentFromOldSolver"] = "true";
  }

  // Handle overrides
  const overridenFFlags = fflagsConfig.get<FFlags>("override");
  if (overridenFFlags) {
    for (let [name, value] of Object.entries(overridenFFlags)) {
      if (!isAlphanumericUnderscore(name)) {
        vscode.window.showWarningMessage(
          `Invalid FFlag name: '${name}'. It can only contain alphanumeric characters`,
        );
      }

      name = name.trim();
      value = value.trim();

      // Strip kind prefix if it was included
      for (const kind of FFLAG_KINDS) {
        if (name.startsWith(`${kind}`)) {
          name = name.substring(kind.length);
        }
      }

      // Validate that the name and value is valid
      if (name.length > 0 && value.length > 0) {
        fflags[name] = value;
      }
    }
  }

  const serverConfiguration =
    vscode.workspace.getConfiguration("luau-lsp.server");

  const serverBinConfig = serverConfiguration.get("path", "").trim();
  const serverBinUri =
    vscode.workspace.workspaceFolders &&
    vscode.workspace.workspaceFolders.length > 0
      ? utils.resolveUri(
          vscode.workspace.workspaceFolders[0].uri,
          serverBinConfig,
        )
      : vscode.Uri.file(serverBinConfig);
  let serverBinPath;

  if (serverBinConfig !== "" && (await utils.exists(serverBinUri))) {
    serverBinPath = serverBinUri.fsPath;
  } else {
    if (serverBinConfig !== "") {
      vscode.window.showWarningMessage(
        `Server binary at path \`${serverBinUri.fsPath}\` does not exist, falling back to bundled binary`,
      );
    }
    serverBinPath = vscode.Uri.joinPath(
      context.extensionUri,
      "bin",
      os.platform() === "win32" ? "server.exe" : "server",
    ).fsPath;
  }

  const transport =
    serverConfiguration.get<"stdio" | "pipe">(
      "communicationChannel",
      "stdio",
    ) === "pipe"
      ? TransportKind.pipe
      : TransportKind.stdio;

  const delayStartup = serverConfiguration.get<boolean>("delayStartup", false);
  if (delayStartup) {
    addArg("--delay-startup");
  }

  if (isCrashReportingEnabled()) {
    addArg("--enable-crash-reporting");
    addArg(
      `--crash-report-directory=${vscode.Uri.joinPath(context.globalStorageUri, "sentry").fsPath}`,
    );
  }

  const run: Executable = {
    command: serverBinPath,
    args,
    transport,
  };

  // If debugging, run the locally build extension, with local type definitions file
  const debug: Executable = {
    command: process.env["LUAU_LSP_SERVER_PATH"]
      ? vscode.Uri.file(process.env["LUAU_LSP_SERVER_PATH"]).fsPath
      : serverBinPath,
    args: debugArgs,
    transport,
  };

  const serverOptions: ServerOptions = { run, debug };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [
      { language: "lua", scheme: "file" },
      { language: "luau", scheme: "file" },
      { language: "lua", scheme: "untitled" },
      { language: "luau", scheme: "untitled" },
    ],
    diagnosticPullOptions: {
      onChange: vscode.workspace
        .getConfiguration("luau-lsp.diagnostics")
        .get("pullOnChange", true),
      onSave: vscode.workspace
        .getConfiguration("luau-lsp.diagnostics")
        .get("pullOnSave", true),
    },
    initializationOptions: {
      fflags,
    },
    markdown: {
      supportHtml: true,
    },
    errorHandler: new ClientErrorHandler(context, 4),
  };

  client = new LanguageClient(
    "luau",
    "Luau Language Server",
    serverOptions,
    clientOptions,
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
    }),
    vscode.commands.registerCommand("luau-lsp.flushTimeTrace", async () => {
      if (client) {
        client.sendNotification("$/flushTimeTrace");
      }
    }),
  );

  context.subscriptions.push(
    vscode.workspace.onDidChangeConfiguration((e) => {
      if (e.affectsConfiguration("luau-lsp.server")) {
        vscode.window
          .showInformationMessage(
            "Luau LSP server configuration has changed, reload server for this to take effect.",
            "Reload Language Server",
          )
          .then((command) => {
            if (command === "Reload Language Server") {
              vscode.commands.executeCommand("luau-lsp.reloadServer");
            }
          });
      } else if (
        e.affectsConfiguration("luau-lsp.fflags") ||
        e.affectsConfiguration("luau-lsp.completion.enableFragmentAutocomplete")
      ) {
        vscode.window
          .showInformationMessage(
            "Luau FFlags have been changed, reload server for this to take effect.",
            "Reload Language Server",
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
            "Reload Language Server",
          )
          .then((command) => {
            if (command === "Reload Language Server") {
              vscode.commands.executeCommand("luau-lsp.reloadServer");
            }
          });
      }
    }),
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
