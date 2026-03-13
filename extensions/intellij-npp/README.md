# Neuron++ JetBrains Plugin

This plugin provides support for the Neuron++ (NPP) language in JetBrains IDEs (IntelliJ IDEA, CLion, etc.) using the Language Server Protocol (LSP).

## Features

- Syntax highlighting for `.nr` files.
- LSP integration for features provided by `neuron-lsp`:
    - Code completion
    - Hover information
    - Go to definition
    - Rename
    - Diagnostics (errors and warnings)

## Prerequisites

- **Neuron++ LSP Server**: You must have `neuron-lsp` built and available on your system.
- **JetBrains IDE**: Version 2023.3 or newer.

## Installation

### From Source

1. Clone the NPP repository.
2. Navigate to `extensions/intellij-npp`.
3. Build the plugin using Gradle:
   ```bash
   ./gradlew buildPlugin
   ```
4. Find the generated plugin ZIP file in `build/distributions/intellij-npp-0.1.0.zip`.
5. In your JetBrains IDE:
   - Go to `Settings` -> `Plugins`.
   - Click the gear icon and select `Install Plugin from Disk...`.
   - Select the generated ZIP file.
   - Restart the IDE if prompted.

### For Developers

To run the plugin in a development instance of the IDE:
```bash
./gradlew runIde
```

## How it Works

The plugin automatically looks for the `neuron-lsp` executable in several locations:
1. Workspace build folders (e.g., `build/bin/neuron-lsp`).
2. Your system's `PATH`.

Once a `.nr` file is opened, the plugin will attempt to start the server and connect via standard I/O.
