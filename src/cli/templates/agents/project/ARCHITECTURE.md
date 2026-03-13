# Project Architecture

This document provides a high-level overview of the project's architecture, module dependencies, and execution flow.

---

## Overview

This is a Neuron project. Neuron programs are organized into modules, where each `.nr` file represents a single module containing exactly one class. The program entry point is the `Init` method in `src/Main.nr`.

---

## Execution Flow

1. **Startup** â€” The runtime initializes core subsystems (memory allocator, thread pool, tensor engine, GPU runtime).
2. **Module Loading** â€” All imported modules are loaded and their dependencies resolved.
3. **Module Initialization** â€” Each module's `Init` method (if present) is called in dependency order.
4. **Entry Point** â€” The `Init` method in `Main.nr` is executed.
5. **Shutdown** â€” Resources are released and the process exits.

---

## Module Dependency Graph

Modules declare dependencies via `module <Name>;` statements at the top of each file. The compiler resolves these into a directed acyclic graph (DAG) for deterministic initialization order.

```
Main.nr
â”œâ”€â”€ module Math
â”œâ”€â”€ module Vector2
â”‚   â””â”€â”€ module Math
â””â”€â”€ module NeuralNetwork
    â”œâ”€â”€ module Tensor
    â””â”€â”€ module Optimizer
```

Circular dependencies between modules are not permitted by the compiler.

---

## Build Pipeline

```
Source (.nr)
    â†“
Lexer â†’ Tokens
    â†“
Parser â†’ AST
    â†“
Semantic Analyzer â†’ Validated AST
    â†“
NIR Builder â†’ Neuron Intermediate Representation
    â†“
   â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
   â†“                â†“                  â†“
LLVM CodeGen    NCON Bytecode     NIR Optimizer
   â†“                â†“                  â†“
Native Binary   .ncon Container   Optimized NIR
```

The project can be built and executed via:
- `neuron build` â€” LLVM-based native compilation
- `neuron run` â€” NCON bytecode container execution (default)
- `neuron ncon watch` â€” Continuous build with hot-reload

---

## Directory Conventions

| Directory    | Purpose                                    |
|--------------|--------------------------------------------|
| `src/`       | All `.nr` source files                    |
| `modules/`   | External package dependencies              |
| `build/`     | Compiled output (binaries, IR, containers) |
| `docs/`      | Project and script documentation           |
| `tests/`     | Automated and unit tests                   |
| `agents/`    | Agent reference documentation (generated)  |

---

## Configuration

Project settings are split across two files:

- **`neuron.toml`** â€” Build configuration, dependencies, NCON settings, and tensor profiles
- **`.neuronsettings`** â€” Source code rules, naming constraints, and lint configuration

See `agents/language/Details/STRUCTURE.md` for full details on project layout.
