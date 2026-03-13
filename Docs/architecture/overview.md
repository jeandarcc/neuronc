# Neuron Compiler Architecture Overview

## What is Neuron?

Neuron (Neuron) is a statically-typed, ahead-of-time compiled systems language
targeting native performance (via LLVM), Vulkan/WebGPU GPU compute, and WASM.
The compiler toolchain consists of three main executables:

| Binary | Purpose |
|--------|---------|
| `neuron` | Main compiler + CLI driver |
| `ncon` | Package manager and build orchestrator |
| `neuron-lsp` | Language Server Protocol server for IDE integration |

---

## Component Map

```
┌─────────────────────────────────────────────────────────────────┐
│                        neuron (CLI driver)                       │
│  src/main/  ─ command dispatch, toolchain utils, build support   │
└──────────────────────────┬──────────────────────────────────────┘
                           │
          ┌────────────────▼────────────────┐
          │         Compiler Pipeline        │
          │  (libneuron + neuronc static lib) │
          └────────────────┬────────────────┘
                           │
    ┌──────────┬───────────┼───────────┬──────────┐
    ▼          ▼           ▼           ▼          ▼
  Lexer     Parser       Sema         NIR        MIR
src/lexer  src/parser  src/sema    src/nir    src/mir
    │          │           │           │          │
    │     src/frontend     │     (optimizer)      │
    │     (source load)    │                      ▼
    │                      │               src/codegen
    │                      │               (LLVM backend)
    └──────────────────────┴──────────────────────┘

┌──────────────────────────────────┐
│  ncon (package manager)          │
│  src/ncon/ — manifest, sandbox,  │
│  VM execution, hot-reload, ncon  │
└──────────────────────────────────┘

┌──────────────────────────────────┐
│  neuron-lsp (language server)    │
│  src/lsp/ — LSP protocol,        │
│  document manager, handlers      │
└──────────────────────────────────┘

┌──────────────────────────────────┐
│  Runtime (C library)             │
│  runtime/src/ — tensor, nn, io,  │
│  graphics, platform, gpu         │
└──────────────────────────────────┘
```

---

## Key Subsystem Roles

### Compiler (`src/`)
- **`lexer/`** — Tokenizes raw source text into a flat `Token` stream.
- **`parser/`** — Builds the AST from tokens; organized into `core/`, `declarations/`, `expressions/`, `statements/` sub-passes.
- **`sema/`** — Type checking, name resolution, ownership analysis, all semantic rules.
- **`nir/`** — Neuron IR: high-level, typed SSA-based IR. Optimizer runs here.
- **`mir/`** — Machine IR: lowers NIR toward LLVM IR representation.
- **`codegen/`** — LLVM IR generation and JIT engine.
- **`diagnostics/`** — Multi-locale error/warning reporting engine.
- **`lsp/`** — Full LSP 3.17 protocol implementation.
- **`ncon/`** — VM execution, manifest parsing, hot-reload, sandbox, bytecode.

### Runtime (`runtime/`)
- **`tensor/`** — Core tensor operations (BLAS-like, modular micro-kernels).
- **`nn/`** — Neural network layer primitives.
- **`io/`** — File and stream I/O.
- **`graphics/`** — 2D/3D scene graph, asset pipeline, Vulkan/WebGPU backends.
- **`platform/`** — Win32/POSIX/Apple/Web abstraction layer.
- **`gpu/`** — CUDA and Vulkan compute backends.

---

## Data Flow Summary

```
Source (.nr)
  → Lexer (Tokens)
  → Parser (AST)
  → Frontend (source + diagnostic setup)
  → SemanticAnalyzer (type-checked AST)
  → NIR Builder (NIR module)
  → Optimizer (NIR optimization passes)
  → MIR Builder (MIR module)
  → LLVMCodeGen (LLVM IR → native binary)
```

See [pipeline.md](pipeline.md) for per-pass details.
