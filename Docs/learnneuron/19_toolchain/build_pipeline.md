# Build Pipeline

Neuron supports two compilation paths.

---

## Path 1: LLVM Native Compilation

For production builds with maximum performance:

```
Source (.nr)
   â”‚
   â”œâ”€â”€ Lexer â†’ Tokens
   â”œâ”€â”€ Parser â†’ AST
   â”œâ”€â”€ Semantic Analysis â†’ Validated AST
   â”œâ”€â”€ NIR Builder â†’ NIR (Neuron IR)
   â”œâ”€â”€ LLVM CodeGen â†’ LLVM IR
   â”œâ”€â”€ LLVM Optimizer â†’ Optimized IR
   â””â”€â”€ LLVM Backend â†’ Native Binary (.exe)
```

Command: `neuron build`

---

## Path 2: NCON Bytecode Container

For development, portability, and hot-reload:

```
Source (.nr)
   â”‚
   â”œâ”€â”€ Lexer â†’ Tokens
   â”œâ”€â”€ Parser â†’ AST
   â”œâ”€â”€ Semantic Analysis â†’ Validated AST
   â”œâ”€â”€ NIR Builder â†’ NIR
   â”œâ”€â”€ Bytecode Lowerer â†’ NCON Bytecode
   â””â”€â”€ Builder â†’ .ncon Container â†’ VM Execution
```

Command: `neuron run` or `neuron ncon build`

---

## Comparison

| Feature | LLVM Native | NCON |
|---------|------------|------|
| Command | `neuron build` | `neuron run` |
| Output | `.exe` binary | `.ncon` container |
| Performance | Maximum | Good |
| Portability | Platform-specific | Cross-platform |
| Hot Reload | âŒ | âœ… |
| Sandbox | âŒ | âœ… |

---

## Debug Pipeline

For compiler development and debugging:

```bash
neuron lex file.nr        # Stage 1: Tokens
neuron parse file.nr      # Stage 2: AST
neuron nir file.nr        # Stage 3: NIR
neuron compile file.nr    # Stage 4: LLVM IR
```

---

## Next Steps

- [Advanced](../20_advanced/unsafe.md) â€” Low-level features
