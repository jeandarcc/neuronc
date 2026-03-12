const cp = require("child_process");
const fs = require("fs");
const path = require("path");
const vscode = require("vscode");

function isNppDocument(document) {
  return document && document.languageId === "npp";
}

function lspSeverityToVsCode(severity) {
  switch (severity) {
    case 1:
      return vscode.DiagnosticSeverity.Error;
    case 2:
      return vscode.DiagnosticSeverity.Warning;
    case 3:
      return vscode.DiagnosticSeverity.Information;
    case 4:
      return vscode.DiagnosticSeverity.Hint;
    default:
      return vscode.DiagnosticSeverity.Error;
  }
}

function contentToMarkdownString(contents) {
  if (contents == null) {
    return null;
  }
  if (typeof contents === "string") {
    return new vscode.MarkdownString(contents);
  }
  if (Array.isArray(contents)) {
    return new vscode.MarkdownString(
      contents
        .map((item) => (typeof item === "string" ? item : item.value || ""))
        .join("\n\n")
    );
  }
  if (typeof contents === "object" && typeof contents.value === "string") {
    return new vscode.MarkdownString(contents.value);
  }
  return null;
}

function rangeFromLsp(range) {
  const start = range?.start || { line: 0, character: 0 };
  const end = range?.end || start;
  return new vscode.Range(
    new vscode.Position(start.line, start.character),
    new vscode.Position(end.line, end.character)
  );
}

function lspRangeFromVsCode(range) {
  return {
    start: {
      line: range.start.line,
      character: range.start.character,
    },
    end: {
      line: range.end.line,
      character: range.end.character,
    },
  };
}

function workspaceEditFromLsp(edit) {
  if (!edit) {
    return null;
  }

  const workspaceEdit = new vscode.WorkspaceEdit();
  const changes = edit.changes || {};
  for (const [uri, edits] of Object.entries(changes)) {
    const resource = vscode.Uri.parse(uri);
    for (const textEdit of edits || []) {
      workspaceEdit.replace(resource, rangeFromLsp(textEdit.range), textEdit.newText || "");
    }
  }
  return workspaceEdit;
}

function lspInlayHintKindToVsCode(kind) {
  return kind === 1 ? vscode.InlayHintKind.Type : vscode.InlayHintKind.Parameter;
}

function diagnosticToLsp(diagnostic) {
  return {
    range: lspRangeFromVsCode(diagnostic.range),
    message: diagnostic.message || "",
    code:
      typeof diagnostic.code === "object" && diagnostic.code
        ? diagnostic.code.value
        : diagnostic.code,
    source: diagnostic.source,
    severity: diagnostic.severity,
  };
}

function workspaceSymbolFromLsp(item) {
  if (!item?.location?.uri) {
    return null;
  }
  return new vscode.SymbolInformation(
    item.name || "",
    item.kind ?? vscode.SymbolKind.Variable,
    item.containerName || "",
    new vscode.Location(
      vscode.Uri.parse(item.location.uri),
      rangeFromLsp(item.location.range)
    )
  );
}

function callHierarchyItemFromLsp(item) {
  if (!item?.uri) {
    return null;
  }
  const hierarchyItem = new vscode.CallHierarchyItem(
    item.kind ?? vscode.SymbolKind.Function,
    item.name || "",
    item.detail || "",
    vscode.Uri.parse(item.uri),
    rangeFromLsp(item.range),
    rangeFromLsp(item.selectionRange)
  );
  hierarchyItem._lspData = item.data;
  return hierarchyItem;
}

function callHierarchyItemToLsp(item) {
  return {
    name: item.name,
    kind: item.kind,
    detail: item.detail,
    uri: item.uri.toString(),
    range: lspRangeFromVsCode(item.range),
    selectionRange: lspRangeFromVsCode(item.selectionRange),
    data: item._lspData,
  };
}

function typeHierarchyItemFromLsp(item) {
  if (!item?.uri) {
    return null;
  }
  const hierarchyItem = new vscode.TypeHierarchyItem(
    item.kind ?? vscode.SymbolKind.Class,
    item.name || "",
    item.detail || "",
    vscode.Uri.parse(item.uri),
    rangeFromLsp(item.range),
    rangeFromLsp(item.selectionRange)
  );
  hierarchyItem._lspData = item.data;
  return hierarchyItem;
}

function typeHierarchyItemToLsp(item) {
  return {
    name: item.name,
    kind: item.kind,
    detail: item.detail,
    uri: item.uri.toString(),
    range: lspRangeFromVsCode(item.range),
    selectionRange: lspRangeFromVsCode(item.selectionRange),
    data: item._lspData,
  };
}

function codeActionKindFromLsp(kind) {
  return kind === "quickfix" ? vscode.CodeActionKind.QuickFix : kind;
}

function codeActionFromLsp(item) {
  if (!item?.title) {
    return null;
  }

  const action = new vscode.CodeAction(
    item.title,
    codeActionKindFromLsp(item.kind) || vscode.CodeActionKind.QuickFix
  );
  if (item.edit) {
    action.edit = workspaceEditFromLsp(item.edit);
  }
  if (Array.isArray(item.diagnostics)) {
    action.diagnostics = item.diagnostics.map((entry) => {
      const diagnostic = new vscode.Diagnostic(
        rangeFromLsp(entry.range),
        entry.message || "",
        lspSeverityToVsCode(entry.severity)
      );
      if (entry.code) {
        diagnostic.code = entry.code;
      }
      if (entry.source) {
        diagnostic.source = entry.source;
      }
      return diagnostic;
    });
  }
  action.isPreferred = Boolean(item.isPreferred);
  return action;
}

const semanticTokenLegend = new vscode.SemanticTokensLegend(
  ["namespace", "type", "function", "parameter", "variable", "property"],
  []
);

const DEBUG_VIEW_SCHEME = "neuron-debug";

class DebugViewContentProvider {
  constructor(client) {
    this.client = client;
    this.emitter = new vscode.EventEmitter();
    this.onDidChange = this.emitter.event;
  }

  dispose() {
    this.emitter.dispose();
  }

  buildUri(sourceUri, viewKind) {
    const source = vscode.Uri.parse(sourceUri);
    const fileName =
      path.basename(source.fsPath || source.path || "document.npp") || "document.npp";
    const extension = viewKind === "expanded" ? "npp" : "txt";
    return vscode.Uri.from({
      scheme: DEBUG_VIEW_SCHEME,
      path: `/${fileName}.${viewKind}.${extension}`,
      query: `source=${encodeURIComponent(sourceUri)}&view=${encodeURIComponent(
        viewKind
      )}`,
    });
  }

  parseUri(uri) {
    const params = new URLSearchParams(uri.query || "");
    return {
      sourceUri: params.get("source"),
      viewKind: params.get("view") || "expanded",
    };
  }

  async provideTextDocumentContent(uri) {
    const { sourceUri, viewKind } = this.parseUri(uri);
    if (!sourceUri) {
      return "Debug view unavailable.\n";
    }
    const response = await this.client.sendRequest("neuron/textDocument/debugView", {
      textDocument: { uri: sourceUri },
      view: viewKind,
    });
    return response?.content || "Debug view unavailable.\n";
  }

  refreshForSource(sourceUri) {
    for (const document of vscode.workspace.textDocuments) {
      if (document.uri.scheme !== DEBUG_VIEW_SCHEME) {
        continue;
      }
      const info = this.parseUri(document.uri);
      if (info.sourceUri === sourceUri) {
        this.emitter.fire(document.uri);
      }
    }
  }
}

class NeuronLspClient {
  constructor(context) {
    this.context = context;
    this.process = null;
    this.nextId = 1;
    this.pending = new Map();
    this.buffer = Buffer.alloc(0);
    this.diagnostics = vscode.languages.createDiagnosticCollection("neuron");
    this.output = vscode.window.createOutputChannel("Neuron LSP");
    this.hoverProvider = null;
    this.renameProvider = null;
    this.inlayHintsProvider = null;
    this.semanticTokensProvider = null;
    this.workspaceSymbolProvider = null;
    this.callHierarchyProvider = null;
    this.typeHierarchyProvider = null;
    this.codeActionProvider = null;
    this.codeLensProvider = null;
    this.debugViewProvider = new DebugViewContentProvider(this);
    this.debugViewRegistration = null;
    this.eventDisposables = [];
  }

  dispose() {
    this.stop();
    this.diagnostics.dispose();
    this.output.dispose();
    this.debugViewProvider.dispose();
  }

  async start() {
    const server = this.resolveServerCommand();
    if (!server) {
      this.output.appendLine("neuron-lsp executable not found.");
      return;
    }

    this.process = cp.spawn(server.command, server.args, {
      cwd: server.cwd,
      stdio: "pipe",
    });

    this.process.stdout.on("data", (chunk) => this.onStdout(chunk));
    this.process.stderr.on("data", (chunk) => {
      this.output.append(chunk.toString("utf8"));
    });
    this.process.on("error", (error) => {
      this.output.appendLine(`Failed to start neuron-lsp: ${error.message}`);
      this.process = null;
    });
    this.process.on("exit", (code) => {
      this.output.appendLine(`neuron-lsp exited with code ${code}`);
      this.process = null;
      for (const pending of this.pending.values()) {
        pending.reject(new Error("neuron-lsp exited"));
      }
      this.pending.clear();
    });

    this.installEditorBindings();

    const rootUri =
      vscode.workspace.workspaceFolders &&
      vscode.workspace.workspaceFolders.length > 0
        ? vscode.workspace.workspaceFolders[0].uri.toString()
        : null;

    await this.sendRequest("initialize", {
      processId: process.pid,
      rootUri,
      capabilities: {
        textDocument: {
          hover: {
            contentFormat: ["markdown", "plaintext"],
          },
          synchronization: {
            didSave: false,
            dynamicRegistration: false,
          },
        },
      },
    });
    this.sendNotification("initialized", {});

    for (const document of vscode.workspace.textDocuments) {
      if (isNppDocument(document)) {
        this.didOpen(document);
      }
    }
  }

  stop() {
    for (const disposable of this.eventDisposables) {
      disposable.dispose();
    }
    this.eventDisposables = [];
    if (this.hoverProvider) {
      this.hoverProvider.dispose();
      this.hoverProvider = null;
    }
    if (this.renameProvider) {
      this.renameProvider.dispose();
      this.renameProvider = null;
    }
    if (this.inlayHintsProvider) {
      this.inlayHintsProvider.dispose();
      this.inlayHintsProvider = null;
    }
    if (this.semanticTokensProvider) {
      this.semanticTokensProvider.dispose();
      this.semanticTokensProvider = null;
    }
    if (this.workspaceSymbolProvider) {
      this.workspaceSymbolProvider.dispose();
      this.workspaceSymbolProvider = null;
    }
    if (this.callHierarchyProvider) {
      this.callHierarchyProvider.dispose();
      this.callHierarchyProvider = null;
    }
    if (this.typeHierarchyProvider) {
      this.typeHierarchyProvider.dispose();
      this.typeHierarchyProvider = null;
    }
    if (this.codeActionProvider) {
      this.codeActionProvider.dispose();
      this.codeActionProvider = null;
    }
    if (this.codeLensProvider) {
      this.codeLensProvider.dispose();
      this.codeLensProvider = null;
    }
    if (this.debugViewRegistration) {
      this.debugViewRegistration.dispose();
      this.debugViewRegistration = null;
    }
    if (this.process) {
      try {
        this.sendRequest("shutdown", {}).catch(() => {});
        this.sendNotification("exit", {});
      } catch (_) {}
      this.process.kill();
      this.process = null;
    }
    this.pending.clear();
    this.buffer = Buffer.alloc(0);
    this.diagnostics.clear();
  }

  resolveServerCommand() {
    const configured = vscode.workspace
      .getConfiguration()
      .get("neuron.lsp.path", "")
      .trim();
    if (configured) {
      return {
        command: configured,
        args: [],
        cwd: this.defaultCwd(),
      };
    }

    const folders = vscode.workspace.workspaceFolders || [];
    for (const folder of folders) {
      const folderName = path.basename(folder.uri.fsPath);
      const localAppData = process.env.LOCALAPPDATA || "";
      const candidates = [
        path.join(folder.uri.fsPath, "build", "bin", "neuron-lsp.exe"),
        path.join(folder.uri.fsPath, "build", "bin", "neuron-lsp"),
        path.join(folder.uri.fsPath, "build-mingw", "bin", "neuron-lsp.exe"),
        path.join(folder.uri.fsPath, "build-mingw", "bin", "neuron-lsp"),
      ];
      // Also check the canonical NeuronPP workspace state root
      if (localAppData) {
        candidates.push(
          path.join(localAppData, "NeuronPP", "workspaces", folderName, "build-mingw", "bin", "neuron-lsp.exe"),
          path.join(localAppData, "NeuronPP", "workspaces", folderName, "build-mingw", "bin", "neuron-lsp"),
          path.join(localAppData, "NeuronPP", "workspaces", folderName, "build", "bin", "neuron-lsp.exe"),
          path.join(localAppData, "NeuronPP", "workspaces", folderName, "build", "bin", "neuron-lsp"),
        );
      }
      for (const candidate of candidates) {
        if (fs.existsSync(candidate)) {
          return {
            command: candidate,
            args: [],
            cwd: folder.uri.fsPath,
          };
        }
      }
    }

    return {
      command: process.platform === "win32" ? "neuron-lsp.exe" : "neuron-lsp",
      args: [],
      cwd: this.defaultCwd(),
    };
  }

  defaultCwd() {
    const folders = vscode.workspace.workspaceFolders || [];
    return folders.length > 0 ? folders[0].uri.fsPath : undefined;
  }

  installEditorBindings() {
    if (!this.debugViewRegistration) {
      this.debugViewRegistration = vscode.workspace.registerTextDocumentContentProvider(
        DEBUG_VIEW_SCHEME,
        this.debugViewProvider
      );
    }

    this.eventDisposables.push(
      vscode.workspace.onDidOpenTextDocument((document) => {
        if (isNppDocument(document)) {
          this.didOpen(document);
        }
      })
    );
    this.eventDisposables.push(
      vscode.workspace.onDidChangeTextDocument((event) => {
        if (isNppDocument(event.document)) {
          this.didChange(event);
        }
      })
    );
    this.eventDisposables.push(
      vscode.workspace.onDidCloseTextDocument((document) => {
        if (isNppDocument(document)) {
          this.didClose(document);
        }
      })
    );
    this.eventDisposables.push(
      vscode.commands.registerCommand("neuron.openDebugView", async (sourceUri, viewKind) => {
        const activeDocument = vscode.window.activeTextEditor?.document;
        let targetUri =
          typeof sourceUri === "string" && sourceUri
            ? vscode.Uri.parse(sourceUri)
            : activeDocument?.uri;
        if (
          targetUri &&
          targetUri.scheme === DEBUG_VIEW_SCHEME &&
          activeDocument?.uri.scheme === DEBUG_VIEW_SCHEME
        ) {
          const info = this.debugViewProvider.parseUri(activeDocument.uri);
          targetUri = info.sourceUri ? vscode.Uri.parse(info.sourceUri) : undefined;
        }
        const targetView = typeof viewKind === "string" && viewKind ? viewKind : "nir";
        if (!targetUri) {
          return;
        }
        const previewUri = this.debugViewProvider.buildUri(targetUri.toString(), targetView);
        const previewDocument = await vscode.workspace.openTextDocument(previewUri);
        await vscode.window.showTextDocument(previewDocument, {
          preview: true,
          viewColumn: vscode.ViewColumn.Beside,
        });
      })
    );
    this.eventDisposables.push(
      vscode.commands.registerCommand("neuron.showExpandedSource", async () => {
        await vscode.commands.executeCommand("neuron.openDebugView", undefined, "expanded");
      })
    );
    this.eventDisposables.push(
      vscode.commands.registerCommand("neuron.showNir", async () => {
        await vscode.commands.executeCommand("neuron.openDebugView", undefined, "nir");
      })
    );
    this.eventDisposables.push(
      vscode.commands.registerCommand("neuron.showMir", async () => {
        await vscode.commands.executeCommand("neuron.openDebugView", undefined, "mir");
      })
    );

    this.hoverProvider = vscode.languages.registerHoverProvider("npp", {
      provideHover: async (document, position) => {
        const response = await this.sendRequest("textDocument/hover", {
          textDocument: { uri: document.uri.toString() },
          position: {
            line: position.line,
            character: position.character,
          },
        });
        if (!response || !response.contents) {
          return null;
        }
        const markdown = contentToMarkdownString(response.contents);
        if (!markdown) {
          return null;
        }
        return new vscode.Hover(markdown);
      },
    });

    this.renameProvider = vscode.languages.registerRenameProvider("npp", {
      provideRenameEdits: async (document, position, newName) => {
        const response = await this.sendRequest("textDocument/rename", {
          textDocument: { uri: document.uri.toString() },
          position: {
            line: position.line,
            character: position.character,
          },
          newName,
        });
        return workspaceEditFromLsp(response);
      },
    });

    this.workspaceSymbolProvider = vscode.languages.registerWorkspaceSymbolProvider({
      provideWorkspaceSymbols: async (query) => {
        const response = await this.sendRequest("workspace/symbol", { query });
        return (response || []).map(workspaceSymbolFromLsp).filter(Boolean);
      },
    });

    this.callHierarchyProvider = vscode.languages.registerCallHierarchyProvider(
      "npp",
      {
        prepareCallHierarchy: async (document, position) => {
          const response = await this.sendRequest(
            "textDocument/prepareCallHierarchy",
            {
              textDocument: { uri: document.uri.toString() },
              position: {
                line: position.line,
                character: position.character,
              },
            }
          );
          const items = Array.isArray(response)
            ? response
            : response
            ? [response]
            : [];
          return items.map(callHierarchyItemFromLsp).filter(Boolean);
        },
        provideCallHierarchyIncomingCalls: async (item) => {
          const response = await this.sendRequest("callHierarchy/incomingCalls", {
            item: callHierarchyItemToLsp(item),
          });
          return (response || [])
            .map((entry) => {
              const from = callHierarchyItemFromLsp(entry.from);
              if (!from) {
                return null;
              }
              return new vscode.CallHierarchyIncomingCall(
                from,
                (entry.fromRanges || []).map(rangeFromLsp)
              );
            })
            .filter(Boolean);
        },
        provideCallHierarchyOutgoingCalls: async (item) => {
          const response = await this.sendRequest("callHierarchy/outgoingCalls", {
            item: callHierarchyItemToLsp(item),
          });
          return (response || [])
            .map((entry) => {
              const to = callHierarchyItemFromLsp(entry.to);
              if (!to) {
                return null;
              }
              return new vscode.CallHierarchyOutgoingCall(
                to,
                (entry.fromRanges || []).map(rangeFromLsp)
              );
            })
            .filter(Boolean);
        },
      }
    );

    this.typeHierarchyProvider = vscode.languages.registerTypeHierarchyProvider(
      "npp",
      {
        prepareTypeHierarchy: async (document, position) => {
          const response = await this.sendRequest(
            "textDocument/prepareTypeHierarchy",
            {
              textDocument: { uri: document.uri.toString() },
              position: {
                line: position.line,
                character: position.character,
              },
            }
          );
          const items = Array.isArray(response)
            ? response
            : response
            ? [response]
            : [];
          return items.map(typeHierarchyItemFromLsp).filter(Boolean);
        },
        provideTypeHierarchySupertypes: async (item) => {
          const response = await this.sendRequest("typeHierarchy/supertypes", {
            item: typeHierarchyItemToLsp(item),
          });
          return (response || []).map(typeHierarchyItemFromLsp).filter(Boolean);
        },
        provideTypeHierarchySubtypes: async (item) => {
          const response = await this.sendRequest("typeHierarchy/subtypes", {
            item: typeHierarchyItemToLsp(item),
          });
          return (response || []).map(typeHierarchyItemFromLsp).filter(Boolean);
        },
      }
    );

    this.inlayHintsProvider = vscode.languages.registerInlayHintsProvider("npp", {
      provideInlayHints: async (document, range) => {
        const response = await this.sendRequest("textDocument/inlayHint", {
          textDocument: { uri: document.uri.toString() },
          range: {
            start: {
              line: range.start.line,
              character: range.start.character,
            },
            end: {
              line: range.end.line,
              character: range.end.character,
            },
          },
        });
        return (response || []).map((item) => {
          const position = item.position || { line: 0, character: 0 };
          const hint = new vscode.InlayHint(
            new vscode.Position(position.line, position.character),
            item.label || "",
            lspInlayHintKindToVsCode(item.kind)
          );
          hint.paddingLeft = Boolean(item.paddingLeft);
          hint.paddingRight = Boolean(item.paddingRight);
          if (item.tooltip) {
            hint.tooltip =
              typeof item.tooltip === "string"
                ? new vscode.MarkdownString(item.tooltip)
                : undefined;
          }
          return hint;
        });
      },
    });

    this.semanticTokensProvider =
      vscode.languages.registerDocumentSemanticTokensProvider(
        "npp",
        {
          provideDocumentSemanticTokens: async (document) => {
            const response = await this.sendRequest(
              "textDocument/semanticTokens/full",
              {
                textDocument: { uri: document.uri.toString() },
              }
            );
            const data = Array.isArray(response?.data) ? response.data : [];
            return new vscode.SemanticTokens(Uint32Array.from(data));
          },
        },
        semanticTokenLegend
      );

    this.codeActionProvider = vscode.languages.registerCodeActionsProvider(
      "npp",
      {
        provideCodeActions: async (document, range, context) => {
          const response = await this.sendRequest("textDocument/codeAction", {
            textDocument: { uri: document.uri.toString() },
            range: lspRangeFromVsCode(range),
            context: {
              diagnostics: context.diagnostics.map(diagnosticToLsp),
              only: context.only ? context.only.value || String(context.only) : undefined,
            },
          });
          return (response || []).map(codeActionFromLsp).filter(Boolean);
        },
      },
      {
        providedCodeActionKinds: [vscode.CodeActionKind.QuickFix],
      }
    );

    this.codeLensProvider = vscode.languages.registerCodeLensProvider("npp", {
      provideCodeLenses: async (document) => {
        const response = await this.sendRequest("textDocument/codeLens", {
          textDocument: { uri: document.uri.toString() },
        });
        return (response || []).map((item) => {
          const command = item.command
            ? {
                title: item.command.title || "",
                command: item.command.command || "",
                tooltip: item.command.tooltip,
                arguments: item.command.arguments || [],
              }
            : undefined;
          return new vscode.CodeLens(rangeFromLsp(item.range), command);
        });
      },
    });
  }

  didOpen(document) {
    this.sendNotification("textDocument/didOpen", {
      textDocument: {
        uri: document.uri.toString(),
        languageId: document.languageId,
        version: document.version,
        text: document.getText(),
      },
    });
    this.debugViewProvider.refreshForSource(document.uri.toString());
  }

  didChange(event) {
    this.sendNotification("textDocument/didChange", {
      textDocument: {
        uri: event.document.uri.toString(),
        version: event.document.version,
      },
      contentChanges: event.contentChanges.map((change) => ({
        range: change.range
          ? {
              start: {
                line: change.range.start.line,
                character: change.range.start.character,
              },
              end: {
                line: change.range.end.line,
                character: change.range.end.character,
              },
            }
          : undefined,
        text: change.text,
      })),
    });
    this.debugViewProvider.refreshForSource(event.document.uri.toString());
  }

  didClose(document) {
    this.sendNotification("textDocument/didClose", {
      textDocument: {
        uri: document.uri.toString(),
      },
    });
    this.diagnostics.delete(document.uri);
    this.debugViewProvider.refreshForSource(document.uri.toString());
  }

  onStdout(chunk) {
    this.buffer = Buffer.concat([this.buffer, Buffer.from(chunk)]);
    while (true) {
      const headerEnd = this.buffer.indexOf("\r\n\r\n");
      if (headerEnd < 0) {
        return;
      }

      const headerText = this.buffer.slice(0, headerEnd).toString("ascii");
      const match = headerText.match(/Content-Length:\s*(\d+)/i);
      if (!match) {
        this.buffer = this.buffer.slice(headerEnd + 4);
        continue;
      }

      const contentLength = Number(match[1]);
      const messageStart = headerEnd + 4;
      const messageEnd = messageStart + contentLength;
      if (this.buffer.length < messageEnd) {
        return;
      }

      const payload = this.buffer
        .slice(messageStart, messageEnd)
        .toString("utf8");
      this.buffer = this.buffer.slice(messageEnd);
      this.handleMessage(JSON.parse(payload));
    }
  }

  handleMessage(message) {
    if (Object.prototype.hasOwnProperty.call(message, "id")) {
      const pending = this.pending.get(message.id);
      if (!pending) {
        return;
      }
      this.pending.delete(message.id);
      if (message.error) {
        pending.reject(new Error(message.error.message || "LSP error"));
      } else {
        pending.resolve(message.result);
      }
      return;
    }

    if (message.method === "textDocument/publishDiagnostics") {
      const params = message.params || {};
      const uri = vscode.Uri.parse(params.uri);
      const diagnostics = (params.diagnostics || []).map((item) => {
        const diagnostic = new vscode.Diagnostic(
          rangeFromLsp(item.range),
          item.message || "",
          lspSeverityToVsCode(item.severity)
        );
        if (item.code) {
          diagnostic.code = item.code;
        }
        if (item.source) {
          diagnostic.source = item.source;
        }
        return diagnostic;
      });
      this.diagnostics.set(uri, diagnostics);
    }
  }

  sendNotification(method, params) {
    this.sendMessage({
      jsonrpc: "2.0",
      method,
      params,
    });
  }

  sendRequest(method, params) {
    if (!this.process) {
      return Promise.resolve(null);
    }
    const id = this.nextId++;
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
      this.sendMessage({
        jsonrpc: "2.0",
        id,
        method,
        params,
      });
    });
  }

  sendMessage(message) {
    if (!this.process || !this.process.stdin.writable) {
      return;
    }
    const payload = Buffer.from(JSON.stringify(message), "utf8");
    const header = Buffer.from(
      `Content-Length: ${payload.length}\r\n\r\n`,
      "ascii"
    );
    this.process.stdin.write(Buffer.concat([header, payload]));
  }
}

let client = null;

async function activate(context) {
  client = new NeuronLspClient(context);
  context.subscriptions.push(client);
  await client.start();
}

function deactivate() {
  if (client) {
    client.dispose();
    client = null;
  }
}

module.exports = {
  activate,
  deactivate,
};
