# NIR (Neuron Intermediate Representation)

NIR is the compiler's internal representation between the AST and final code generation.

---

## Pipeline Position

```
Source â†’ Lexer â†’ Parser â†’ AST â†’ Semantic Analysis â†’ NIR â†’ {LLVM IR | NCON Bytecode}
```

NIR sits between semantic analysis and code generation, serving both the LLVM and NCON paths.

---

## Viewing NIR

```bash
neuron nir file.nr
```

---

## What NIR Contains

- **Functions** â€” Typed, validated method representations
- **Types** â€” Resolved type information
- **Constants** â€” Evaluated constant expressions
- **Instructions** â€” Simplified operations (no sugar syntax)

---

## Why NIR?

| Benefit | Description |
|---------|-------------|
| **Shared** | Both LLVM and NCON paths consume NIR |
| **Optimizable** | Optimizations applied at NIR level benefit both paths |
| **Portable** | Language-level IR independent of target platform |
| **Debuggable** | Human-readable with `neuron nir` command |

---

## Next Steps

- [Optimizations](optimizations.md) â€” Compiler optimization passes
