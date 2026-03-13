# Installation

This guide covers how to install the Neuron compiler and toolchain on your system.

---

## System Requirements

| Component | Requirement |
|-----------|-------------|
| **OS** | Windows 10+, Linux (x64), macOS (ARM64) |
| **Disk Space** | ~200 MB for compiler + toolchain |
| **RAM** | 4 GB minimum, 8 GB recommended |
| **Toolchain** | Bundled MinGW/GCC (Windows) or system GCC/Clang |

---

## Installation Steps

### Windows

1. Download the latest Neuron release archive
2. Extract to your preferred location (e.g., `C:\Neuron`)
3. Add the `bin/` directory to your system `PATH`
4. Verify the installation:

```bash
neuron help
```

The bundled toolchain includes MinGW GCC â€” no separate compiler installation is required.

### Linux

1. Download the Linux release archive
2. Extract and add to PATH:

```bash
tar -xzf Neuron-linux-x64.tar.gz
export PATH="$HOME/Neuron/bin:$PATH"
neuron help
```

### macOS (ARM64)

1. Download the macOS release
2. Extract and add to PATH:

```bash
tar -xzf Neuron-macos-arm64.tar.gz
export PATH="$HOME/Neuron/bin:$PATH"
neuron help
```

---

## Verifying the Installation

Run the following command to confirm everything works:

```bash
neuron help
```

You should see the Neuron ASCII banner and a list of available commands.

---

## Environment Variables

| Variable | Purpose |
|----------|---------|
| `NEURON_TOOLCHAIN_BIN` | Override the bundled GCC/binutils directory |
| `NEURON_TRACE_ERRORS` | Enable detailed source-context error traces (`1` or `true`) |
| `NEURON_COLOR` | Enable/disable colored diagnostics (`0` or `1`) |
| `NO_COLOR` | Disable colors (standard convention) |
| `NEURON_RUNTIME_CACHE_DIR` | Override runtime object cache location |

---

## Next Steps

- [Hello World](hello_world.md) â€” Create your first Neuron project
