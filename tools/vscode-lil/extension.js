const vscode = require("vscode");
const path = require("path");
const os = require("os");
const { execFile } = require("child_process");

function activate(context) {
  const formatCmd = path.join(os.homedir(), "dev/lil/tools/lil_format.py");

  const provider = vscode.languages.registerDocumentFormattingEditProvider("lil", {
    provideDocumentFormattingEdits(document) {
      return new Promise((resolve, reject) => {
        const src = document.getText();
        const proc = execFile("python3", [formatCmd], { maxBuffer: 10 * 1024 * 1024 }, (err, stdout) => {
          if (err) {
            vscode.window.showErrorMessage("lil format failed: " + err.message);
            reject(err);
            return;
          }
          const range = new vscode.Range(
            document.positionAt(0),
            document.positionAt(src.length)
          );
          resolve([new vscode.TextEdit(range, stdout)]);
        });
        proc.stdin.write(src);
        proc.stdin.end();
      });
    },
  });

  context.subscriptions.push(provider);

  const cmd = vscode.commands.registerCommand("lil-format.formatFile", () => {
    vscode.commands.executeCommand("editor.action.formatDocument");
  });
  context.subscriptions.push(cmd);
}

function deactivate() {}

module.exports = { activate, deactivate };
