# Build Pipeline

Neuron++ supports two compilation paths.

---

## Path 1: LLVM Native Compilation

For production builds with maximum performance:

```
Source (.npp)
   │
   ├── Lexer → Tokens
   ├── Parser → AST
   ├── Semantic Analysis → Validated AST
   ├── NIR Builder → NIR (Neuron IR)
   ├── LLVM CodeGen → LLVM IR
   ├── LLVM Optimizer → Optimized IR
   └── LLVM Backend → Native Binary (.exe)
```

Command: `neuron build`

---

## Path 2: NCON Bytecode Container

For development, portability, and hot-reload:

```
Source (.npp)
   │
   ├── Lexer → Tokens
   ├── Parser → AST
   ├── Semantic Analysis → Validated AST
   ├── NIR Builder → NIR
   ├── Bytecode Lowerer → NCON Bytecode
   └── Builder → .ncon Container → VM Execution
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
| Hot Reload | ❌ | ✅ |
| Sandbox | ❌ | ✅ |

---

## Debug Pipeline

For compiler development and debugging:

```bash
neuron lex file.npp        # Stage 1: Tokens
neuron parse file.npp      # Stage 2: AST
neuron nir file.npp        # Stage 3: NIR
neuron compile file.npp    # Stage 4: LLVM IR
```

---

## Next Steps

- [Advanced](../20_advanced/unsafe.md) — Low-level features
