# Project Structure

Every Neuron project follows a standard directory layout created by `neuron new`.

---

## Directory Layout

```
my_project/
â”‚
â”œâ”€â”€ neuron.toml              # Project configuration and build settings
â”œâ”€â”€ .neuronsettings          # Source code rules and lint configuration
â”œâ”€â”€ .gitignore               # Git exclusion rules
â”‚
â”œâ”€â”€ src/                     # Source code
â”‚   â””â”€â”€ Main.nr             # Program entry point (Init method)
â”‚
â”œâ”€â”€ modules/                 # External package dependencies
â”‚
â”œâ”€â”€ build/                   # Build output (auto-generated)
â”‚
â”œâ”€â”€ docs/                    # Documentation
â”‚   â””â”€â”€ scripts/             # Per-script documentation
â”‚       â””â”€â”€ Main.md          # Docs for Main.nr
â”‚
â”œâ”€â”€ tests/                   # Tests
â”‚   â”œâ”€â”€ auto/                # Automated integration tests
â”‚   â””â”€â”€ unit/                # Unit tests
â”‚
â””â”€â”€ agents/                  # Agent reference (generated, gitignored)
    â”œâ”€â”€ language/             # Language guides
    â””â”€â”€ project/              # Project-specific docs
```

---

## Key Files

### `neuron.toml`

The primary project configuration file:

```toml
[project]
name = "my_project"
version = "0.1.0"

[build]
main = "src/Main.nr"
build_dir = "build"
optimize = "aggressive"
emit_ir = "optimized"
target_cpu = "native"
tensor_profile = "balanced"
tensor_autotune = true
tensor_kernel_cache = "build/.neuron_cache/tensor/"

[dependencies]
# Add packages here

[ncon]
hot_reload = false
```

### `.neuronsettings`

Controls source code rules enforced by the compiler:

```ini
max_classes_per_file = 1
max_lines_per_file = 1000
require_method_uppercase_start = true
enforce_strict_file_naming = true
max_lines_per_method = 50
max_nesting_depth = 3
require_script_docs = true
require_script_docs_min_lines = 5
require_public_method_docs = true
```

See [.neuronsettings Reference](../19_toolchain/neuronsettings.md) for all available settings.

### `src/Main.nr`

The program entry point. Must contain an `Init` method:

```npp
Init method() {
    Print("Hello Neuron!");
}
```

### `docs/scripts/Main.md`

Documentation for `Main.nr`. Required when `require_script_docs = true`. Must have at least 5 non-empty lines (configurable).

---

## Source Organization

Each `.nr` file contains exactly **one class**. The class name must match the filename:

```
src/
â”œâ”€â”€ Main.nr          â†’ Init method (entry point)
â”œâ”€â”€ Vector2.nr       â†’ Vector2 class
â”œâ”€â”€ Player.nr        â†’ Player class
â””â”€â”€ AI/
    â”œâ”€â”€ Brain.nr     â†’ Brain class
    â””â”€â”€ Neuron.nr    â†’ Neuron class
```

Subdirectories are allowed for organizing larger projects.

---

## Build Output

After `neuron build` or `neuron run`:

```
build/
â”œâ”€â”€ Main.ll           # LLVM IR (if emit_ir configured)
â”œâ”€â”€ Main.obj          # Compiled object file
â”œâ”€â”€ Main.exe          # Native executable
â””â”€â”€ .neuron_cache/    # Tensor kernel cache
```

---

## .gitignore Defaults

```
build/
Build/
agents/
```

The `agents/` directory is regenerated from templates on project creation.

---

## Next Steps

- [Variables](../01_variables/declaration.md) â€” Start writing Neuron code
