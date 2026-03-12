# Public C/C++ API (`include/`)

This directory contains the single source of truth for the Neuron++ public
Application Binary Interface (ABI).

All headers in this directory must map to the `neuronc` namespace (i.e., they sit
inside the `include/neuronc/` subdirectory).

## The ABI Boundary Rule

> **Never put internal implementation details in `include/neuronc/`.**

Any C++ class or struct defined here is considered part of the compiler's public
API. It can be linked against by:
- IDE extensions (`extensions/`)
- Package manager plugins (`MorphAPI/`)
- User-authored pass plugins (`ncon` native extensions)

If a class definition needs to change its memory layout (adding/removing fields),
it breaks the ABI. Therefore, types exposed here heavily favor opaque pointers
or Pimpl structures where possible, especially in the AST and NIR subsystems.

## Key Directories under `include/neuronc/`

| Directory | Purpose |
|-----------|---------|
| `ast/` | Public interfaces for traversing the Abstract Syntax Tree. |
| `diagnostics/` | Structs for the `DiagnosticEmitter` and error handling API. |
| `ir/` | Public interfaces for inspecting NIR/MIR modules (read-only views). |
| `sema/` | Read-only views into the semantic `AnalysisContext` and `SymbolTable`. |
| `frontend/` | The compiler driver API (used by `neuron` CLI and `neuron-lsp`). |
| `cli/` | Command-line parsing and config structures. |
| `ncon/` | Package metadata and sandbox VM interaction structures. |
