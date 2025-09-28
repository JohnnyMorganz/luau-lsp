/* eslint-disable @typescript-eslint/naming-convention */
import path from "path";
import * as vscode from "vscode";
import {
  LanguageClient,
  RequestType,
  TextDocumentIdentifier,
} from "vscode-languageclient/node";

export type RequireGraphParams = {
  textDocument: TextDocumentIdentifier;
  fromTextDocumentOnly: boolean;
};

export const RequireGraphRequest = new RequestType<
  RequireGraphParams,
  string,
  void
>("luau-lsp/requireGraph");

// Based off https://github.com/rust-lang/rust-analyzer/blob/f5e049d09dc17d0b61de2ec179b3607cf1e431b2/editors/code/src/commands.ts
// Licensed under MIT
const isLuauDocument = (document: vscode.TextDocument) => {
  return document.languageId === "luau" && document.uri.scheme === "file";
};

const isLuauEditor = (editor: vscode.TextEditor) => {
  return isLuauDocument(editor.document);
};

const viewRequireGraph = async (
  context: vscode.ExtensionContext,
  client: LanguageClient,
  document: vscode.TextDocument,
  params: { fromTextDocumentOnly: boolean },
) => {
  const dot = await client.sendRequest(RequireGraphRequest, {
    textDocument:
      client.code2ProtocolConverter.asTextDocumentIdentifier(document),
    ...params,
  });

  const nodeModulesPath = vscode.Uri.file(
    path.join(context.extensionPath, "node_modules"),
  );
  const panel = vscode.window.createWebviewPanel(
    "luau-lsp.require-graph",
    "Luau Require Graph",
    vscode.ViewColumn.Two,
    {
      enableScripts: true,
      retainContextWhenHidden: true,
      localResourceRoots: [nodeModulesPath],
    },
  );
  const uri = panel.webview.asWebviewUri(nodeModulesPath);

  const html = `
            <!DOCTYPE html>
            <meta charset="utf-8">
            <head>
                <style>
                    /* Fill the entire view */
                    html, body { margin:0; padding:0; overflow:hidden }
                    svg { position:fixed; top:0; left:0; height:100%; width:100% }

                    /* Disable the graphviz background and fill the polygons */
                    .graph > polygon { display:none; }
                    :is(.node,.edge) polygon { fill: white; }

                    /* Invert the line colours for dark themes */
                    body:not(.vscode-light) .edge path { stroke: white; }
                </style>
            </head>
            <body>
                <script type="text/javascript" src="${uri}/d3/dist/d3.min.js"></script>
                <script type="text/javascript" src="${uri}/@hpcc-js/wasm/dist/graphviz.umd.js"></script>
                <script type="text/javascript" src="${uri}/d3-graphviz/build/d3-graphviz.min.js"></script>
                <div id="graph"></div>
                <script>
                    let dot = \`${dot}\`;
                    let graph = d3.select("#graph")
                                  .graphviz({ useWorker: false, useSharedWorker: false })
                                  .fit(true)
                                  .zoomScaleExtent([0.1, Infinity])
                                  .renderDot(dot);

                    d3.select(window).on("click", (event) => {
                        if (event.ctrlKey) {
                            graph.resetZoom(d3.transition().duration(100));
                        }
                    });
                    d3.select(window).on("copy", (event) => {
                        event.clipboardData.setData("text/plain", dot);
                        event.preventDefault();
                    });
                </script>
            </body>
            `;

  panel.webview.html = html;
};

export const registerRequireGraph = (
  context: vscode.ExtensionContext,
  client: LanguageClient,
) => {
  return [
    vscode.commands.registerTextEditorCommand(
      "luau-lsp.requireGraph",
      async (textEditor: vscode.TextEditor) => {
        if (isLuauEditor(textEditor)) {
          return viewRequireGraph(context, client, textEditor.document, {
            fromTextDocumentOnly: false,
          });
        }
      },
    ),
    vscode.commands.registerTextEditorCommand(
      "luau-lsp.requireGraphForFile",
      async (textEditor: vscode.TextEditor) => {
        if (isLuauEditor(textEditor)) {
          return viewRequireGraph(context, client, textEditor.document, {
            fromTextDocumentOnly: true,
          });
        }
      },
    ),
  ];
};
