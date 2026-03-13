# Neuron Compiler Source (`src/`)

This directory contains the core implementation of the Neuron compiler pipeline,
the Language Server Protocol (LSP) server, and the `ncon` package manager.

## Architecture Pipeline

The compilation pipeline is strictly staged. Each directory corresponds to a
specific compiler pass or subsystem:

| Subsystem | Input | Output | Description |
|-----------|-------|--------|-------------|
| [`lexer/`](lexer/) | Source bytes | `Token` stream | Linear tokenization |
| [`parser/`](parser/) | `Token` stream | AST | Recursive-descent AST building |
| [`frontend/`](frontend/) | CLI args | Context | Source loading, include resolution |
| [`sema/`](sema/) | AST | Typed AST | Name resolution, type checking, ownership |
| [`nir/`](nir/) | Typed AST | NIR Module | Neuron IR building & optimization passes |
| [`mir/`](mir/) | NIR Module | MIR Module | Machine IR lowering (pre-codegen) |
| [`codegen/`](codegen/) | MIR Module | Native Code | LLVM IR emission & OrcJIT execution |

## Tooling Subsystems

| Directory | Description |
|-----------|-------------|
| [`lsp/`](lsp/) | Full LSP 3.17 server implementation (`neuron-lsp`) |
| [`ncon/`](ncon/) | Package manager, manifest parser, sandbox VM |
| [`diagnostics/`](diagnostics/) | Multi-locale error and warning emitter |
| [`cli/`](cli/) | Command-line dispatch and argument parsing |
| [`main/`](main/) | Entry points and high-level orchestration |

## Design Rules for `src/`

1. **ABI Boundary:** No public headers exist here. All `.h` files in `src/` are
   internal implementation details. The public API lives in `include/neuronc/`.
2. **No Cycles:** Dependencies flow downwards: `codegen` → `mir` → `nir` → `sema` → `parser` → `lexer`.
   A lower layer must never `#include` a header from a higher layer.
3. **Diagnostics:** Never use `std::cerr` or `printf` for errors. Alwasy use the
   `DiagnosticEmitter` with a locale-aware code (e.g., `N2042`).
