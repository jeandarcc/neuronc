﻿# Official IDE Extensions (`extensions/`)

This directory houses the source code for the official IDE extensions that provide
language intelligence for Neuron.

These extensions are thin clients. They primarily launch the `neuron-lsp` binary
(built from `src/lsp/`) to provide real-time code analysis, error reporting,
and intellisense.

## Projects

| Extension | Subdirectory | Tech Stack | Status |
|-----------|--------------|------------|--------|
| **VS Code** | `vscode-neuron/` | TypeScript, npm | Stable. Includes custom AST/NIR explorer debug views. |
| **IntelliJ** | `intellij-neuron/` | Kotlin, Gradle | Alpha. Uses JetBrains LSP framework. |

## Building the VS Code Extension

```bash
cd extensions/vscode-neuron
npm install
npm run package
```

This produces a `.vsix` file which can be installed in VS Code via the Command Palette:
`Extensions: Install from VSIX...`

### Debugging (VS Code)

1. Open `extensions/vscode-npp/` in a fresh VS Code window.
2. Press `F5`. This will launch an "Extension Development Host" window with the extension active.
3. The extension will automatically look for `neuron-lsp.exe` in your `PATH` or in the standard build output directory (`%LOCALAPPDATA%\Neuron\workspaces\Neuron\build-mingw\bin\`).

## Adding Syntax Highlighting Features

Syntax highlighting is defined in `extensions/vscode-npp/syntaxes/neuron.tmLanguage.json`.
1. Edit the grammar file.
2. Use the `Developer: Inspect Editor Tokens and Scopes` command in VS Code to see how your changes map to tokens.
3. Reload the Extension Development Host window to test.
