# What Is Neuron++?

Neuron++ (NPP) is a high-performance, compiled programming language designed for artificial intelligence, scientific computing, and GPU-accelerated workloads.

---

## Design Goals

Neuron++ is built around four core principles:

| Principle | Description |
|-----------|-------------|
| **C++ Performance** | Compiles to native machine code — no interpreter overhead |
| **Readability** | Natural-language keywords instead of cryptic symbols |
| **Simple Memory Model** | Alias-by-default with explicit copy and move controls |
| **GPU & Tensor Native** | First-class tensor types and `gpu { }` execution blocks |

---

## How It Works

Neuron++ supports two execution paths:

### 1. LLVM Native Compilation

For maximum-performance production builds:

```
Source (.npp) → Lexer → Parser → AST → Semantic Analysis → NIR → LLVM IR → Native Binary
```

This path produces platform-native executables that run at full hardware speed.

### 2. NCON Bytecode Containers

For portable, sandboxed, and hot-reloadable execution:

```
Source (.npp) → Lexer → Parser → AST → NIR → Bytecode → .ncon Container → VM
```

NCON containers run inside a sandboxed virtual machine with security isolation and live hot-reload support. This is the default path used by `neuron run`.

---

## Key Features

- **`is` keyword** for variable binding (optional for methods and classes)
- **`another`** for deep copies, **`move`** for ownership transfer
- **`address of` / `value of`** — readable pointer syntax
- **One class per file** — enforced by the compiler
- **`gpu { }`** blocks with automatic CPU fallback
- **First-class tensors** with `@` matrix multiplication
- **Generics** with type constraints
- **`async` / `await`** concurrency
- **`try` / `catch` / `finally`** error handling
- **NCON containers** for portable bytecode execution
- **Hot reload** via `neuron ncon watch`

---

## A Quick Look

```npp
module Math;

Init method() {
    x is 42;
    y is another x;
    name is "Neuron++" as string;

    Print(name);
    Print(x + y);
}
```

This program:
1. Imports the `Math` module
2. Declares `x` as an alias-bound integer
3. Creates `y` as an independent copy of `x`
4. Prints `"Neuron++"` and `84`

---

## Next Steps

- [Installation](installation.md) — Set up the Neuron++ toolchain
- [Hello World](hello_world.md) — Create and run your first project
- [Project Structure](project_structure.md) — Understand the directory layout
