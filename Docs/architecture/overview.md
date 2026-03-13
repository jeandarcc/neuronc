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
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚                        neuron (CLI driver)                       â”‚
â”‚  src/main/  â”€ command dispatch, toolchain utils, build support   â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
                           â”‚
          â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â–¼â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
          â”‚         Compiler Pipeline        â”‚
          â”‚  (libneuron + neuronc static lib) â”‚
          â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
                           â”‚
    â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¼â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
    â–¼          â–¼           â–¼           â–¼          â–¼
  Lexer     Parser       Sema         NIR        MIR
src/lexer  src/parser  src/sema    src/nir    src/mir
    â”‚          â”‚           â”‚           â”‚          â”‚
    â”‚     src/frontend     â”‚     (optimizer)      â”‚
    â”‚     (source load)    â”‚                      â–¼
    â”‚                      â”‚               src/codegen
    â”‚                      â”‚               (LLVM backend)
    â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”´â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜

â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚  ncon (package manager)          â”‚
â”‚  src/ncon/ â€” manifest, sandbox,  â”‚
â”‚  VM execution, hot-reload, ncon  â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜

â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚  neuron-lsp (language server)    â”‚
â”‚  src/lsp/ â€” LSP protocol,        â”‚
â”‚  document manager, handlers      â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜

â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚  Runtime (C library)             â”‚
â”‚  runtime/src/ â€” tensor, nn, io,  â”‚
â”‚  graphics, platform, gpu         â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
```

---

## Key Subsystem Roles

### Compiler (`src/`)
- **`lexer/`** â€” Tokenizes raw source text into a flat `Token` stream.
- **`parser/`** â€” Builds the AST from tokens; organized into `core/`, `declarations/`, `expressions/`, `statements/` sub-passes.
- **`sema/`** â€” Type checking, name resolution, ownership analysis, all semantic rules.
- **`nir/`** â€” Neuron IR: high-level, typed SSA-based IR. Optimizer runs here.
- **`mir/`** â€” Machine IR: lowers NIR toward LLVM IR representation.
- **`codegen/`** â€” LLVM IR generation and JIT engine.
- **`diagnostics/`** â€” Multi-locale error/warning reporting engine.
- **`lsp/`** â€” Full LSP 3.17 protocol implementation.
- **`ncon/`** â€” VM execution, manifest parsing, hot-reload, sandbox, bytecode.

### Runtime (`runtime/`)
- **`tensor/`** â€” Core tensor operations (BLAS-like, modular micro-kernels).
- **`nn/`** â€” Neural network layer primitives.
- **`io/`** â€” File and stream I/O.
- **`graphics/`** â€” 2D/3D scene graph, asset pipeline, Vulkan/WebGPU backends.
- **`platform/`** â€” Win32/POSIX/Apple/Web abstraction layer.
- **`gpu/`** â€” CUDA and Vulkan compute backends.

---

## Data Flow Summary

```
Source (.nr)
  â†’ Lexer (Tokens)
  â†’ Parser (AST)
  â†’ Frontend (source + diagnostic setup)
  â†’ SemanticAnalyzer (type-checked AST)
  â†’ NIR Builder (NIR module)
  â†’ Optimizer (NIR optimization passes)
  â†’ MIR Builder (MIR module)
  â†’ LLVMCodeGen (LLVM IR â†’ native binary)
```

See [pipeline.md](pipeline.md) for per-pass details.
