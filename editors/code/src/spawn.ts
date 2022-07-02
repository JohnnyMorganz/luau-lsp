import * as vscode from "vscode";

import {
  ChildProcessWithoutNullStreams,
  SpawnOptionsWithoutStdio,
  spawn,
} from "child_process";

type ChildProcessRejection = {
  interactive: boolean;
  code: number;
  reason: string;
};

const canSpawnInteractive = (platform: NodeJS.Platform): boolean => {
  switch (platform) {
    case "darwin":
      return true;
    case "linux":
      return true;
    default:
      return false;
  }
};

const handleChildProcessPromise = (
  cp: ChildProcessWithoutNullStreams,
  resolve: (value: string) => void,
  reject: (reason: ChildProcessRejection) => void,
  interactive: boolean
) => {
  // Set stdout and stderr encoding + pipe to plain strings
  let stdout = "";
  let stderr = "";
  cp.stdout.setEncoding("utf8");
  cp.stderr.setEncoding("utf8");
  cp.stdout.on("data", (data) => (stdout += data));
  cp.stderr.on("data", (data) => (stderr += data));
  // Reject if spawning errors out
  cp.on("error", (e) => {
    reject({
      interactive,
      code: -1,
      reason: e.message,
    });
  });
  // Resolve with stdout or reject with stderr
  // depending on the exist code and/or data
  cp.on("close", (code) => {
    if (code === 0 && stderr.length === 0) {
      resolve(stdout);
    } else {
      reject({
        interactive,
        code: code || -1,
        reason: stderr,
      });
    }
  });
};

const spawnNaive = (
  command: string,
  args: string[],
  options?: SpawnOptionsWithoutStdio
): Promise<string> => {
  return new Promise((resolve, reject) => {
    // Spawn the wanted command directly
    const cp = spawn(command, args, {
      cwd: process.cwd(),
      env: process.env,
      ...options,
    });
    // Hand the child process over and
    // resolve / reject when it closes
    handleChildProcessPromise(cp, resolve, reject, false);
  });
};

const spawnInteractive = (
  command: string,
  args: string[],
  options?: SpawnOptionsWithoutStdio
): Promise<string> => {
  return new Promise((resolve, reject) => {
    // Spawn the configured vscode shell as if it is a
    // normal interactive shell + set the proper env
    // vars needed for an interactive shell
    let cwd = process.cwd();
    if (options && typeof options.cwd === "string") {
      cwd = options.cwd;
    }
    const cp = spawn(vscode.env.shell, {
      cwd: cwd,
      env: {
        // eslint-disable-next-line @typescript-eslint/naming-convention
        PWD: cwd,
        // eslint-disable-next-line @typescript-eslint/naming-convention
        TERM: "xterm",
        ...process.env,
      },
      ...options,
    });
    // Hand the child process over and
    // resolve / reject when it closes
    handleChildProcessPromise(cp, resolve, reject, true);
    // Write the command input, newline, then end the
    // input stream to let the shell exit on its own
    cp.stdin.write(`${command} ${args.join(" ")}`);
    cp.stdin.write("\n");
    cp.stdin.end();
  });
};

const wrappedSpawn = (
  command: string,
  args: string[],
  options?: SpawnOptionsWithoutStdio
): Promise<string> => {
  return new Promise((resolve, reject) => {
    spawnNaive(command, args, options)
      .then(resolve)
      .catch((err) => {
        if (!err.ran && canSpawnInteractive(process.platform)) {
          spawnInteractive(command, args, options).then(resolve).catch(reject);
        } else {
          reject(err);
        }
      });
  });
};

export default wrappedSpawn;
