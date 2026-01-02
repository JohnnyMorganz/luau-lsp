import { AbstractMessageReader, AbstractMessageWriter, MessageWriter, MessageReader, Message } from 'vscode-jsonrpc';
import { MonacoLanguageClient } from 'monaco-languageclient';
import type { DataCallback, Disposable, MessageTransports } from 'vscode-languageclient';
import * as monaco from 'monaco-editor';
import * as vscode from 'vscode';
import 'vscode/localExtensionHost';
import { initialize } from 'vscode/services';

// the language server as a web worker
const LSP_FILE_LOCATION = '/Luau.LanguageServer.Web.js';

const fileContents = `-- A work in progress for luau-lsp integration into Monaco
-- To demonstrate the documentation and definition file formats, this demo includes a 'demo' module with one function and three global types.
-- This current version has some issues.
-- When updating to the latest versions of monaco and codingame's monaco libraries, I somehow lost configuration which allows error squiggles to work.
-- Additionally, documentation isn't loading as intended for the @demo package.

local fake_val = demo.fake_function({foo = "hello", bar = 123})
local wrong = math.abs("hello, world")
function broken
  return wrong
without end
`;
await initialize({});

export type WorkerLoader = () => Worker
const workerLoaders: Partial<Record<string, WorkerLoader>> = {
  editorWorkerService: () => new Worker(new URL('monaco-editor/esm/vs/editor/editor.worker.js', import.meta.url), { type: 'module' }),
}
window.MonacoEnvironment = {
  getWorker: function (moduleId, label) {
    const workerFactory = workerLoaders[label]
    if (workerFactory != null) {
      return workerFactory()
    }
    throw new Error(`Unimplemented worker ${label} (${moduleId})`)
  }
}

const model = monaco.editor.createModel(fileContents, 'luau');

monaco.languages.register({
    id: 'luau',
    extensions: ['.lua', '.luau'],
    aliases: ['Luau', 'luau', 'Lua', 'lua'],
});

monaco.editor.create(document.getElementById('editor')!, {
    model: model,
    language: 'luau',
    tabSize: 2
});

let scriptWorker : Worker;

const createScriptWorker = async function() {
    await fetch(LSP_FILE_LOCATION);
    const worker = new Worker(LSP_FILE_LOCATION, {type: 'module'});
    return worker;
}

class WasmMessageWriter extends AbstractMessageWriter implements MessageWriter {
    protected port : Worker;
    constructor(port : Worker) {
        super();
        this.port = port;
    }
    end(): void {
        // todo: shut down the webworker.
        throw new Error('Method not implemented.');
    }
    write(msg : Message) {
        const stringified = JSON.stringify(msg);
        console.log('Monaco=>LangServ', stringified);
        this.port.postMessage(stringified);
        return Promise.resolve();
    }
}

class WasmMessageReader extends AbstractMessageReader implements MessageReader {
    protected callbacks : DataCallback[] = [];
    constructor(port : Worker) {
        super();
        port.onmessage = x => {
            if(x.data.type == 'output') {
                this.publish(x.data.msg);
            } else if(x.data.type=='error') {
                console.error(x.data.exc);
            } else {
                console.log(x.data.msg);
            }
        };
    }
    publish(data : string) {
        const parsed = JSON.parse(data);
        for(const callback of this.callbacks) {
            callback(parsed);
        }
        console.log('LangServ=>Monaco', data);
    }
    listen(callback: DataCallback): Disposable {
        this.callbacks.push(callback);
        return {
            dispose: () => this.callbacks = []
        };
    }
}

const createWasmConnection = async function(_encoding : string) {
    try {
        scriptWorker ??= await createScriptWorker();
        const clientReader = new WasmMessageReader(scriptWorker);
        const clientWriter = new WasmMessageWriter(scriptWorker);
        const transports : MessageTransports = {
            reader: clientReader,
            writer: clientWriter,
        };
        return transports;
    } catch(e) {
        console.error('xxxkr exception encountered while connecting to wasm language server', e);
        throw e;
    }
}



const languageClient = new MonacoLanguageClient({
    name: 'Luau Language Client',
    clientOptions: {
        documentSelector: ['luau'],
        workspaceFolder: {
            index: 0,
            name: 'fake_fs',
            uri: vscode.Uri.parse('/workspace'),
        }
    },
    // create a language client connection from the JSON RPC connection on demand
    connectionProvider: {
        get: createWasmConnection
    }
});

try {
    await languageClient.start();
} catch(e) {
    console.error(e);
}
// hypothetical shutdown
// modelRef.dispose()
// editor.dispose()
// overlayDisposable.dispose()