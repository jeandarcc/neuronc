# NCON Overview

NCON (Neuron Container) is a portable bytecode container format for running Neuron programs in a sandboxed virtual machine.

---

## What Is NCON?

NCON is an alternative execution path to LLVM native compilation:

```
Source (.nr) â†’ NIR â†’ Bytecode â†’ .ncon Container â†’ VM Execution
```

---

## Why NCON?

| Feature | Native (LLVM) | NCON |
|---------|--------------|------|
| Performance | Maximum | Good (bytecode VM) |
| Portability | Platform-specific | Cross-platform |
| Security | Full OS access | Sandboxed |
| Hot Reload | âŒ No | âœ… Yes |
| File Extension | `.exe` / binary | `.ncon` |

---

## Default Execution

`neuron run` uses the NCON path by default for development:

```bash
neuron run          # builds and runs via NCON VM
neuron build        # builds native LLVM binary
```

---

## Container Structure

A `.ncon` file is a binary container with:

- **Header** â€” Magic number, version, flags
- **Module table** â€” List of compiled modules
- **Bytecode** â€” Compiled VM instructions
- **Constants** â€” String/numeric constant pool
- **Metadata** â€” Source maps for debugging

---

## Next Steps

- [Build & Run](build_run.md) â€” Building and running NCON containers
- [Watch & Hot Reload](watch_hotreload.md) â€” Live development
