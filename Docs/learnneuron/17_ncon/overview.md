# NCON Overview

NCON (Neuron Container) is a portable bytecode container format for running Neuron++ programs in a sandboxed virtual machine.

---

## What Is NCON?

NCON is an alternative execution path to LLVM native compilation:

```
Source (.npp) → NIR → Bytecode → .ncon Container → VM Execution
```

---

## Why NCON?

| Feature | Native (LLVM) | NCON |
|---------|--------------|------|
| Performance | Maximum | Good (bytecode VM) |
| Portability | Platform-specific | Cross-platform |
| Security | Full OS access | Sandboxed |
| Hot Reload | ❌ No | ✅ Yes |
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

- **Header** — Magic number, version, flags
- **Module table** — List of compiled modules
- **Bytecode** — Compiled VM instructions
- **Constants** — String/numeric constant pool
- **Metadata** — Source maps for debugging

---

## Next Steps

- [Build & Run](build_run.md) — Building and running NCON containers
- [Watch & Hot Reload](watch_hotreload.md) — Live development
