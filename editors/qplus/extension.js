const vscode = require("vscode");
const { spawn } = require("child_process");
const fs = require("fs");
const path = require("path");

function isFile(p) {
  try {
    return fs.statSync(p).isFile();
  } catch {
    return false;
  }
}

function isDir(p) {
  try {
    return fs.statSync(p).isDirectory();
  } catch {
    return false;
  }
}

function compilerBinDirs(qpcPath) {
  const dirs = [];
  if (path.isAbsolute(qpcPath)) {
    dirs.push(path.dirname(qpcPath));
    const cache = path.join(path.dirname(qpcPath), "CMakeCache.txt");
    if (isFile(cache)) {
      const text = fs.readFileSync(cache, "utf8");
      const match = /^CMAKE_CXX_COMPILER:(?:STRING|FILEPATH)=(.+)$/m.exec(text);
      if (match) {
        dirs.push(path.dirname(match[1].trim()));
      }
    }
  }
  return dirs.filter((d, i, all) => all.indexOf(d) === i && isDir(d));
}

function spawnEnv(qpcPath) {
  const env = { ...process.env };
  const extra = compilerBinDirs(qpcPath);
  if (extra.length === 0) {
    return env;
  }
  const key = Object.keys(env).find((k) => k.toLowerCase() === "path") || "PATH";
  env[key] = extra.join(path.delimiter) + path.delimiter + (env[key] || "");
  return env;
}

function exitMessage(code) {
  const status = code < 0 ? code + 0x100000000 : code;
  if (status === 0xc0000135) {
    return "exited 0xC0000135 missing MinGW DLLs (libstdc++-6.dll); compiler bin is not on PATH";
  }
  return `exited ${code}`;
}

function resolveQpc() {
  const configured = (vscode.workspace.getConfiguration("qplus").get("qpcPath", "qpc") || "qpc").trim();
  if (path.isAbsolute(configured) && isFile(configured)) {
    return configured;
  }
  const names = ["qpc.exe", "qpc"];
  const dirs = ["cmake-build-debug", "cmake-build-release", "build"];
  for (const folder of vscode.workspace.workspaceFolders || []) {
    for (const dir of dirs) {
      for (const name of names) {
        const candidate = path.join(folder.uri.fsPath, dir, name);
        if (isFile(candidate)) {
          return candidate;
        }
      }
    }
  }
  return configured;
}

class QpcClient {
  constructor() {
    this.proc = null;
    this.buffer = Buffer.alloc(0);
    this.nextId = 1;
    this.pending = new Map();
    this.diagnostics = vscode.languages.createDiagnosticCollection("qplus");
    this.ready = false;
    this.failed = false;
  }

  start() {
    const cmd = resolveQpc();
    const cwd = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
    try {
      this.proc = spawn(cmd, ["lsp"], {
        stdio: ["pipe", "pipe", "pipe"],
        windowsHide: true,
        cwd,
        env: spawnEnv(cmd),
      });
    } catch (err) {
      this.fail(cmd, err);
      return;
    }
    this.proc.on("error", (err) => this.fail(cmd, err));
    this.proc.stdout.on("data", (chunk) => this.onData(chunk));
    this.proc.stderr.on("data", () => {});
    this.proc.on("exit", (code) => {
      if (!this.ready) {
        this.fail(cmd, new Error(exitMessage(code)));
      }
      this.ready = false;
      this.proc = null;
    });
    this.sendRequest("initialize", { processId: process.pid, capabilities: {} })
      .then(() => {
        this.sendNotification("initialized", {});
        this.ready = true;
        for (const doc of vscode.workspace.textDocuments) {
          this.didOpen(doc);
        }
      })
      .catch((err) => this.fail(cmd, new Error(err.message || "initialize failed")));
  }

  fail(cmd, err) {
    if (this.failed) {
      return;
    }
    this.failed = true;
    this.ready = false;
    vscode.window.showErrorMessage(`Q+: could not start '${cmd} lsp' (${err.message}). Highlighting still works.`);
  }

  dispose() {
    this.diagnostics.dispose();
    if (this.proc) {
      this.sendNotification("exit", {});
      this.proc.kill();
      this.proc = null;
    }
  }

  send(msg) {
    if (!this.proc || !this.proc.stdin.writable) {
      return;
    }
    const json = JSON.stringify(msg);
    this.proc.stdin.write(`Content-Length: ${Buffer.byteLength(json, "utf8")}\r\n\r\n${json}`);
  }

  sendNotification(method, params) {
    this.send({ jsonrpc: "2.0", method, params });
  }

  sendRequest(method, params) {
    const id = this.nextId++;
    this.send({ jsonrpc: "2.0", id, method, params });
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
      setTimeout(() => {
        if (this.pending.has(id)) {
          this.pending.delete(id);
          reject(new Error("timeout"));
        }
      }, 8000);
    });
  }

  onData(chunk) {
    this.buffer = Buffer.concat([this.buffer, chunk]);
    while (true) {
      const headerEnd = this.buffer.indexOf("\r\n\r\n");
      if (headerEnd < 0) {
        return;
      }
      const header = this.buffer.slice(0, headerEnd).toString("utf8");
      const match = /Content-Length:\s*(\d+)/i.exec(header);
      if (!match) {
        this.buffer = this.buffer.slice(headerEnd + 4);
        continue;
      }
      const length = Number(match[1]);
      const bodyStart = headerEnd + 4;
      if (this.buffer.length < bodyStart + length) {
        return;
      }
      const body = this.buffer.slice(bodyStart, bodyStart + length).toString("utf8");
      this.buffer = this.buffer.slice(bodyStart + length);
      this.onMessage(JSON.parse(body));
    }
  }

  onMessage(msg) {
    if (msg.method === "textDocument/publishDiagnostics" && msg.params) {
      this.applyDiagnostics(msg.params);
      return;
    }
    if (msg.id != null && this.pending.has(msg.id)) {
      const { resolve, reject } = this.pending.get(msg.id);
      this.pending.delete(msg.id);
      if (msg.error) {
        reject(new Error(msg.error.message || "lsp error"));
      } else {
        resolve(msg.result);
      }
    }
  }

  applyDiagnostics(params) {
    const uri = vscode.Uri.parse(params.uri);
    const items = (params.diagnostics || []).map((d) => {
      const start = new vscode.Position(d.range.start.line, d.range.start.character);
      const end = new vscode.Position(d.range.end.line, d.range.end.character);
      const severity =
        d.severity === 2 ? vscode.DiagnosticSeverity.Warning : vscode.DiagnosticSeverity.Error;
      const diag = new vscode.Diagnostic(new vscode.Range(start, end), d.message, severity);
      diag.source = d.source || "qpc";
      return diag;
    });
    this.diagnostics.set(uri, items);
  }

  isQp(doc) {
    return doc.languageId === "qplus" || doc.fileName.endsWith(".qp");
  }

  didOpen(doc) {
    if (!this.ready || !this.isQp(doc)) {
      return;
    }
    this.sendNotification("textDocument/didOpen", {
      textDocument: { uri: doc.uri.toString(), languageId: "qplus", version: doc.version, text: doc.getText() },
    });
  }

  didChange(doc) {
    if (!this.ready || !this.isQp(doc)) {
      return;
    }
    this.sendNotification("textDocument/didChange", {
      textDocument: { uri: doc.uri.toString(), version: doc.version },
      contentChanges: [{ text: doc.getText() }],
    });
  }

  didClose(doc) {
    if (!this.isQp(doc)) {
      return;
    }
    this.diagnostics.delete(doc.uri);
    if (!this.ready) {
      return;
    }
    this.sendNotification("textDocument/didClose", {
      textDocument: { uri: doc.uri.toString() },
    });
  }

  async completion(doc, position) {
    if (!this.ready || !this.isQp(doc)) {
      return [];
    }
    const result = await this.sendRequest("textDocument/completion", {
      textDocument: { uri: doc.uri.toString() },
      position: { line: position.line, character: position.character },
    });
    const items = Array.isArray(result) ? result : (result && result.items) || [];
    return items.map((item) => {
      const c = new vscode.CompletionItem(item.label, item.kind || vscode.CompletionItemKind.Text);
      c.detail = item.detail;
      return c;
    });
  }

  async hover(doc, position) {
    if (!this.ready || !this.isQp(doc)) {
      return null;
    }
    const result = await this.sendRequest("textDocument/hover", {
      textDocument: { uri: doc.uri.toString() },
      position: { line: position.line, character: position.character },
    });
    if (!result || !result.contents) {
      return null;
    }
    const value = typeof result.contents === "string" ? result.contents : result.contents.value;
    if (!value) {
      return null;
    }
    return new vscode.Hover(value);
  }
}

function activate(context) {
  const client = new QpcClient();
  client.start();
  context.subscriptions.push(client);
  context.subscriptions.push(vscode.workspace.onDidOpenTextDocument((doc) => client.didOpen(doc)));
  context.subscriptions.push(vscode.workspace.onDidChangeTextDocument((e) => client.didChange(e.document)));
  context.subscriptions.push(vscode.workspace.onDidCloseTextDocument((doc) => client.didClose(doc)));
  context.subscriptions.push(
    vscode.languages.registerCompletionItemProvider("qplus", {
      provideCompletionItems(doc, position) {
        return client.completion(doc, position);
      },
    })
  );
  context.subscriptions.push(
    vscode.languages.registerHoverProvider("qplus", {
      provideHover(doc, position) {
        return client.hover(doc, position);
      },
    })
  );
}

function deactivate() {}

module.exports = { activate, deactivate };
