# CLI Reference

Complete reference for the `neuron` command-line tool.

---

## Project Commands

| Command | Description |
|---------|-------------|
| `neuron new <name>` | Create a new project |
| `neuron build` | Build native LLVM binary |
| `neuron run` | Build and run via NCON VM |
| `neuron add <pkg>` | Add a dependency |
| `neuron help` | Show help and available commands |

---

## NCON Commands

| Command | Description |
|---------|-------------|
| `neuron ncon build` | Build NCON container |
| `neuron ncon run` | Run NCON container in VM |
| `neuron ncon watch` | Auto-rebuild + hot-reload |
| `neuron ncon inspect <file>` | Inspect container contents |
| `neuron ncon config` | Show NCON configuration |

---

## Debug Commands

| Command | Description |
|---------|-------------|
| `neuron lex <file>` | Show lexer token output |
| `neuron parse <file>` | Show parser AST output |
| `neuron nir <file>` | Show NIR representation |
| `neuron compile <file>` | Show LLVM IR output |

---

## Build Options

| Command | Description |
|---------|-------------|
| `neuron build` | Full optimized build |
| `neuron build-nucleus` | Stripped cross-compilation build |
| `neuron build-product` | Production release build with installers |

---

## Global Flags

```bash
neuron --version          # Show version
neuron --help             # Show help
```

---

## Next Steps

- [neuron.toml](neuron_toml.md) — Project configuration
- [product_builds.md](product_builds.md) — Production build system
