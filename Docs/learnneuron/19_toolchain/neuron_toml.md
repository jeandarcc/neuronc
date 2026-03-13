# neuron.toml Reference

The `neuron.toml` file is the primary project configuration.

---

## Full Reference

```toml
[project]
name = "my_project"
version = "0.1.0"
description = "A Neuron project"

[build]
main = "src/Main.nr"              # Entry point file
build_dir = "build"                # Output directory
optimize = "aggressive"            # none, basic, aggressive
emit_ir = "optimized"              # none, raw, optimized
target_cpu = "native"              # native, generic
tensor_profile = "balanced"        # minimal, balanced, performance
tensor_autotune = true             # Auto-tune tensor kernels
tensor_kernel_cache = "build/.neuron_cache/tensor/"

[dependencies]
# package_name = "version"

[ncon]
hot_reload = false                 # Enable hot reload
# sandbox settings
```

---

## Sections

### `[project]`
Project identity — name, version, description.

### `[build]`
Compiler settings — optimization level, output format, tensor configuration.

### `[dependencies]`
External package dependencies.

### `[ncon]`
NCON container settings — hot reload, sandbox configuration.

---

## Optimization Levels

| Level | Description |
|-------|-------------|
| `none` | No optimization (fastest compile) |
| `basic` | Standard optimizations |
| `aggressive` | Full LLVM optimization pipeline |

---

## Next Steps

- [.neuronsettings](../21_neuronsettings/overview.md) — Source code rules
