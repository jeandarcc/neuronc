# Project Architecture

This document provides a high-level overview of the project's architecture, module dependencies, and execution flow.

---

## Overview

This is a Neuron++ project. Neuron++ programs are organized into modules, where each `.npp` file represents a single module containing exactly one class. The program entry point is the `Init` method in `src/Main.npp`.

---

## Execution Flow

1. **Startup** — The runtime initializes core subsystems (memory allocator, thread pool, tensor engine, GPU runtime).
2. **Module Loading** — All imported modules are loaded and their dependencies resolved.
3. **Module Initialization** — Each module's `Init` method (if present) is called in dependency order.
4. **Entry Point** — The `Init` method in `Main.npp` is executed.
5. **Shutdown** — Resources are released and the process exits.

---

## Module Dependency Graph

Modules declare dependencies via `module <Name>;` statements at the top of each file. The compiler resolves these into a directed acyclic graph (DAG) for deterministic initialization order.

```
Main.npp
├── module Math
├── module Vector2
│   └── module Math
└── module NeuralNetwork
    ├── module Tensor
    └── module Optimizer
```

Circular dependencies between modules are not permitted by the compiler.

---

## Build Pipeline

```
Source (.npp)
    ↓
Lexer → Tokens
    ↓
Parser → AST
    ↓
Semantic Analyzer → Validated AST
    ↓
NIR Builder → Neuron Intermediate Representation
    ↓
   ┌────────────────┬──────────────────┐
   ↓                ↓                  ↓
LLVM CodeGen    NCON Bytecode     NIR Optimizer
   ↓                ↓                  ↓
Native Binary   .ncon Container   Optimized NIR
```

The project can be built and executed via:
- `neuron build` — LLVM-based native compilation
- `neuron run` — NCON bytecode container execution (default)
- `neuron ncon watch` — Continuous build with hot-reload

---

## Directory Conventions

| Directory    | Purpose                                    |
|--------------|--------------------------------------------|
| `src/`       | All `.npp` source files                    |
| `modules/`   | External package dependencies              |
| `build/`     | Compiled output (binaries, IR, containers) |
| `docs/`      | Project and script documentation           |
| `tests/`     | Automated and unit tests                   |
| `agents/`    | Agent reference documentation (generated)  |

---

## Configuration

Project settings are split across two files:

- **`neuron.toml`** — Build configuration, dependencies, NCON settings, and tensor profiles
- **`.neuronsettings`** — Source code rules, naming constraints, and lint configuration

See `agents/language/Details/STRUCTURE.md` for full details on project layout.
