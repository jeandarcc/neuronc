# NCON Build & Run

Build NCON containers and execute them in the bytecode VM.

---

## Building

```bash
neuron ncon build
```

This compiles all `.nr` source files into a single `.ncon` container file in the build directory.

---

## Running

```bash
neuron ncon run
```

Builds (if needed) and executes the container in the NCON virtual machine.

---

## Combined (Default)

```bash
neuron run
```

Equivalent to `neuron ncon run` — builds and runs in one step.

---

## Build Output

```
build/
├── project.ncon          # The bytecode container
└── .neuron_cache/        # Build cache
```

---

## Configuration

In `neuron.toml`:

```toml
[ncon]
hot_reload = false
```

---

## Next Steps

- [Watch & Hot Reload](watch_hotreload.md) — Live development
- [Inspect](inspect.md) — Examining container contents
