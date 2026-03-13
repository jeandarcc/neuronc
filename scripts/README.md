# Build & Orchestration Scripts (`scripts/`)

This directory contains the **only** supported way to build, test, and package
the Neuron compiler on Windows.

> **Rule:** Never invoke `cmake` manually for routine development. Use these scripts.
> They orchestrate Ninja, LLVM paths, DLL copying, and the workspace cache automatically.

## The Scripts

| Script | Purpose |
|--------|---------|
| `build.bat` | The canonical full-build script. Generates the `build-mingw` directory in the UX-defined workspace cache (`%LOCALAPPDATA%\Neuron\workspaces\`), runs CMake, triggers Ninja, and copies all necessary runtime DLLs. |
| `build_tests.ps1` | The legacy targeted test runner. Connects to the workspace cache and runs the test runner with a string filter. |
| `build_tests_v2.ps1` | The modern test runner. Used by CI. |
| `build_runtime.ps1` | A specialized subset of `build.bat` that only rebuilds the C `runtime/` library without relinking `neuronc`. |
| `build_installer.ps1` | Packages the built binaries, the standard library, and the installer UI into a distributive `.exe` or `.zip`. Run automatically by GitHub Actions. |
| `strip_utf8_bom.py` | A maintenance utility to recursively strip Byte-Order Marks from source files (which break certain older LLVM tools if they leak out of the Neuron frontend). |

### Legacy/Old Scripts
* `original_script.ps1`
* `original_script_cmd.ps1`
These are kept for historical reference but should not be used for daily development.
