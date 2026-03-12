# Getting Started with Neuron++ Development

## Prerequisites

| Tool | Minimum Version | Purpose |
|------|----------------|---------|
| Windows | 10 / 11 | Primary dev platform |
| MSYS2 + MinGW-w64 | Latest | Compiler toolchain |
| LLVM | 15+ | Code generation backend |
| CMake | 3.20+ | Build system |
| Ninja | 1.11+ | Build executor |
| Git | 2.30+ | Version control |

### Install MSYS2 Toolchain

```bash
# In MSYS2 MINGW64 shell:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-llvm mingw-w64-x86_64-libffi \
          mingw-w64-x86_64-cmake
```

---

## Cloning and Building

```powershell
git clone https://github.com/your-org/neuron-pp.git
cd neuron-pp

# Set environment (or add to your shell profile)
$env:LLVM_DIR = "C:\msys64\mingw64\lib\cmake\llvm"
$env:NEURON_TOOLCHAIN_BIN = "C:\msys64\mingw64\bin"

# Full build (compiler + runtime + ncon + tests)
scripts\build.bat
```

The build output lands in `%LOCALAPPDATA%\NeuronPP\workspaces\NPP\build-mingw\bin\`.

---

## Running Tests

```powershell
# Run all tests
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build_tests.ps1 `
  -BuildDir "$env:LOCALAPPDATA\NeuronPP\workspaces\NPP\build-mingw" `
  -Filter "*" `
  -LlvmDir "C:\msys64\mingw64\lib\cmake\llvm"

# Run only sema tests
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build_tests.ps1 `
  -BuildDir "$env:LOCALAPPDATA\NeuronPP\workspaces\NPP\build-mingw" `
  -Filter "sema*" `
  -LlvmDir "C:\msys64\mingw64\lib\cmake\llvm"
```

> ⚠️ **Never invoke CMake directly.** Always use scripts in `scripts/`. See `scripts/README.md`.

---

## VS Code Setup

1. Open the repo folder in VS Code.
2. Install recommended extensions when prompted (`clangd`, `cmake-tools`, `editorconfig`).
3. Press **Ctrl+Shift+B** → select **Build (Full)** to build.
4. Use **Run Tests (Filtered)** task for targeted test runs.

The `compile_commands.json` is generated in the build dir and picked up by clangd automatically.

---

## Repository Layout at a Glance

```
src/          Compiler source (lexer, parser, sema, NIR, MIR, codegen, lsp, ncon)
runtime/      C runtime library (tensor, nn, graphics, platform, gpu)
include/      Public headers (ABI boundary: include/neuronc/)
tests/        Single-binary test suite (mirrors src/ layout)
scripts/      Canonical build/test scripts (the ONLY way to build)
config/       Runtime config and diagnostics catalog
extensions/   IDE plugins (vscode-npp, intellij-npp)
docs/         Architecture docs, ADRs, guides, specs
benchmarks/   Performance benchmarking suite
plans/        Long-horizon design plans
```

---

## Making Your First Change

1. Find the relevant module in `src/` (see [architecture overview](../architecture/overview.md)).
2. Edit the files.
3. Add or update a test in the matching `tests/<module>/` directory.
4. Run `scripts\build.bat` to verify build.
5. Run the targeted test filter to verify your test passes.
6. Open a PR — fill in the `.github/PULL_REQUEST_TEMPLATE.md` checklist.
