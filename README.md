?# Neuron (Neuron)

[![CI](https://github.com/your-org/neuron-pp/actions/workflows/ci.yml/badge.svg)](https://github.com/your-org/neuron-pp/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-MIT-green)](#license)

**Neuron (Neuron)** is a high-performance, statically-typed programming language for native systems programming, neural computations, and real-time graphics — powered by LLVM, Vulkan/CUDA, and a self-hosted package manager (`ncon`).

## Key Features

| Feature | Detail |
|---------|--------|
| ⚡ Native performance | LLVM-backed codegen (x86-64, WASM, future RISC-V) |
| 🧠 Neural-native | First-class tensor types, GPU fusion optimizer, CUDA/Vulkan compute |
| 🎮 Built-in graphics | Vulkan 2D/3D scene graph, WebGPU web target |
| 🔌 Full toolchain | `neuron` compiler + `ncon` package manager + `neuron-lsp` LSP server |
| 🖊️ IDE support | VS Code + IntelliJ extensions included |

## Repository Layout

```
src/              Compiler: lexer → parser → sema → NIR → MIR → codegen + lsp + ncon
runtime/          C runtime: tensor, nn, graphics, platform (win32/linux/macos), gpu
include/          Public API headers (ABI boundary: include/neuronc/)
tests/            Single-binary test suite (mirrors src/ layout)
scripts/          Canonical build/test scripts — the ONLY way to build
config/           Diagnostics catalog (multi-locale TOML) + CLI config
extensions/       IDE plugins: vscode-neuron, intellij-neuron
docs/             Architecture docs, ADRs, guides, specs
benchmarks/       Performance benchmarking suite
plans/            Long-horizon design documents
```

## Getting Started

### Prerequisites

- Windows 10/11 with [MSYS2](https://www.msys2.org/) (MinGW-w64 + LLVM 15+)
- CMake 3.20+, Ninja 1.11+

```bash
# Install toolchain in MSYS2 MINGW64 shell:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-llvm mingw-w64-x86_64-libffi mingw-w64-x86_64-cmake
```

### Build

```powershell
set "LLVM_DIR=C:\msys64\mingw64\lib\cmake\llvm"
set "NEURON_TOOLCHAIN_BIN=C:\msys64\mingw64\bin"
scripts\build.bat
```

> ⚠️ **Do not invoke CMake directly.** Always use `scripts/`. See [`scripts/README.md`](scripts/README.md).

### Run Tests

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build_tests.ps1 `
  -BuildDir "$env:LOCALAPPDATA\Neuron\workspaces\Neuron\build-mingw" `
  -Filter "*" `
  -LlvmDir "C:\msys64\mingw64\lib\cmake\llvm"
```

## Documentation

| Doc | Link |
|-----|------|
| Architecture overview | [docs/architecture/overview.md](docs/architecture/overview.md) |
| Compiler pipeline | [docs/architecture/pipeline.md](docs/architecture/pipeline.md) |
| Language reference | [docs/spec/language_reference.md](docs/spec/language_reference.md) |
| NIR specification | [docs/spec/nir_spec.md](docs/spec/nir_spec.md) |
| Getting started | [docs/guides/getting_started.md](docs/guides/getting_started.md) |
| Adding a diagnostic | [docs/guides/adding_a_diagnostic.md](docs/guides/adding_a_diagnostic.md) |
| Writing a pass | [docs/guides/writing_a_pass.md](docs/guides/writing_a_pass.md) |
| ADR index | [docs/adr/README.md](docs/adr/README.md) |
| Web deployment | [docs/WEB_DEPLOYMENT.md](docs/WEB_DEPLOYMENT.md) |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md), [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md), and [SECURITY.md](SECURITY.md).

## License

MIT License — see [LICENSE](LICENSE).
