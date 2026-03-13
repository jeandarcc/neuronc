?# Neuron (Neuron) Syntax Highlighter

This extension provides syntax highlighting and a minimal language-server client
for the Neuron programming language.

## Installation

1. Copy the `vscode-neuron` folder to your VS Code extensions directory:
   - **Windows:** `%USERPROFILE%\.vscode\extensions\`
   - **macOS/Linux:** `~/.vscode/extensions/`
2. Restart VS Code or reload the window (`Ctrl+R` or search for "Reload Window" in Command Palette).

## Features

- Full support for Neuron keywords (`is`, `another`, `method`, etc.)
- Multi-word keyword support (`address of`, `value of`)
- String literals and escape characters
- Comments (Line and Block)
- Numbers (Integer and Float)
- Function call highlighting
- Diagnostics and hover via `neuron-lsp`

## Language Server

The extension tries to start `neuron-lsp` from one of these locations:

1. `neuron.lsp.path` setting
2. `build/bin/neuron-lsp(.exe)` in the workspace
3. `build-mingw/bin/neuron-lsp(.exe)` in the workspace
4. `neuron-lsp` on `PATH`
