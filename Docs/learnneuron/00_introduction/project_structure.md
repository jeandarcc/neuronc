# Project Structure

Every Neuron++ project follows a standard directory layout created by `neuron new`.

---

## Directory Layout

```
my_project/
│
├── neuron.toml              # Project configuration and build settings
├── .neuronsettings          # Source code rules and lint configuration
├── .gitignore               # Git exclusion rules
│
├── src/                     # Source code
│   └── Main.npp             # Program entry point (Init method)
│
├── modules/                 # External package dependencies
│
├── build/                   # Build output (auto-generated)
│
├── docs/                    # Documentation
│   └── scripts/             # Per-script documentation
│       └── Main.md          # Docs for Main.npp
│
├── tests/                   # Tests
│   ├── auto/                # Automated integration tests
│   └── unit/                # Unit tests
│
└── agents/                  # Agent reference (generated, gitignored)
    ├── language/             # Language guides
    └── project/              # Project-specific docs
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
main = "src/Main.npp"
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

### `src/Main.npp`

The program entry point. Must contain an `Init` method:

```npp
Init method() {
    Print("Hello Neuron!");
}
```

### `docs/scripts/Main.md`

Documentation for `Main.npp`. Required when `require_script_docs = true`. Must have at least 5 non-empty lines (configurable).

---

## Source Organization

Each `.npp` file contains exactly **one class**. The class name must match the filename:

```
src/
├── Main.npp          → Init method (entry point)
├── Vector2.npp       → Vector2 class
├── Player.npp        → Player class
└── AI/
    ├── Brain.npp     → Brain class
    └── Neuron.npp    → Neuron class
```

Subdirectories are allowed for organizing larger projects.

---

## Build Output

After `neuron build` or `neuron run`:

```
build/
├── Main.ll           # LLVM IR (if emit_ir configured)
├── Main.obj          # Compiled object file
├── Main.exe          # Native executable
└── .neuron_cache/    # Tensor kernel cache
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

- [Variables](../01_variables/declaration.md) — Start writing Neuron++ code
