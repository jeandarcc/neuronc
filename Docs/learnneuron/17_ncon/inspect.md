# NCON Inspect

Examine the contents of a compiled `.ncon` container.

---

## Usage

```bash
neuron ncon inspect build/project.ncon
```

---

## Output

The inspect command displays:

- **Header** — Version, flags, creation timestamp
- **Module list** — All compiled modules and their sizes
- **Bytecode stats** — Instruction count, constant pool size
- **Metadata** — Source file mappings, debug info availability

---

## Example Output

```
NCON Container: build/project.ncon
Version: 1.0
Modules: 3
  - Main (142 instructions)
  - Math (89 instructions)
  - Vector2 (256 instructions)
Constants: 47 entries
Total size: 12.4 KB
```

---

## Next Steps

- [Neural Networks](../18_neural_network/model.md) — Building AI models
