# Neuron Project Structure

This document defines the recommended directory layout for Neuron projects and explains the purpose of each directory and configuration file.

---

## 1. Standard Project Layout

When you create a new project with `neuron new <name>`, the following structure is generated:

```
my_project/
â”‚
â”œâ”€ neuron.toml              # Project configuration and build settings
â”œâ”€ .neuronsettings          # Source code rules and lint configuration
â”œâ”€ .gitignore               # Git exclusion rules
â”‚
â”œâ”€ src/                     # Source code directory
â”‚   â””â”€ Main.nr             # Program entry point (Init method)
â”‚
â”œâ”€ modules/                 # External module dependencies
â”‚
â”œâ”€ build/                   # Build output directory (auto-generated)
â”‚
â”œâ”€ docs/                    # Project documentation
â”‚   â””â”€ scripts/             # Per-script documentation files
â”‚       â””â”€ Main.md          # Documentation for Main.nr
â”‚
â”œâ”€ tests/                   # Test suites
â”‚   â”œâ”€ auto/                # Automated integration tests
â”‚   â””â”€ unit/                # Unit test files
â”‚
â””â”€ agents/                  # Agent and tooling reference documents
    â”œâ”€ language/             # Language-level documentation
    â”‚   â”œâ”€ LanguageGuide.md  # Complete language overview
    â”‚   â””â”€ Details/          # In-depth topic guides
    â”‚       â”œâ”€ RULES.md
    â”‚       â”œâ”€ ERROR_GUIDE.md
    â”‚       â”œâ”€ NAMING.md
    â”‚       â”œâ”€ GPU_SEMANTICS.md
    â”‚       â””â”€ STRUCTURE.md
    â””â”€ project/              # Project-specific documentation
        â”œâ”€ ARCHITECTURE.md
        â””â”€ RULES.md
```

---

## 2. Key Directories

### `src/`

Contains all `.nr` source files. The main entry point is always `src/Main.nr`, which must define an `Init` method.

**Organization for larger projects:**

```
src/
â”œâ”€ Main.nr
â”œâ”€ Math/
â”‚   â”œâ”€ Vector2.nr
â”‚   â”œâ”€ Vector3.nr
â”‚   â””â”€ Matrix.nr
â”œâ”€ AI/
â”‚   â”œâ”€ Tensor.nr
â”‚   â”œâ”€ Layer.nr
â”‚   â””â”€ Optimizer.nr
â””â”€ Utils/
    â””â”€ Logger.nr
```

Each `.nr` file defines exactly one class, and the class name must match the filename.

### `modules/`

External module dependencies managed by the package manager. Populated by `neuron add <package>`.

### `build/`

Build output directory. Contains compiled binaries, object files, LLVM IR, and NCON containers. This directory is auto-generated and should be added to `.gitignore`.

### `docs/scripts/`

Per-script documentation files. When `require_script_docs = true` in `.neuronsettings`, every source file must have a corresponding `.md` file here (e.g., `docs/scripts/Main.md` for `src/Main.nr`).

### `tests/`

- **`tests/auto/`** â€” Automated integration tests that run during `neuron release`
- **`tests/unit/`** â€” Unit test files for individual modules

Test files matching patterns in `require_script_docs_exclude` (default: `Test*`) are exempt from documentation requirements.

### `agents/`

Reference documentation for AI agents and tooling. Contains language guides, coding rules, and project-specific architecture notes. This directory is added to `.gitignore` by default since it is regenerated from templates.

---

## 3. Configuration Files

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
# tensor = "1.0"
# math = "1.0"

[ncon]
hot_reload = false
# resources = [...]
# [ncon.native.modules.<name>]
# manifest_path = "..."
# source_dir = "..."
```

### `.neuronsettings`

Source code rules and lint configuration. Controls naming conventions, file length limits, documentation requirements, and other compiler checks. See `Details/RULES.md` for the full reference.

### `.gitignore`

Default exclusions:

```
build/
Build/
agents/
```

The `agents/` directory is excluded because it is generated from templates during project creation.

---

## 4. File Naming Rules

| Category        | Convention                    | Example                    |
|-----------------|-------------------------------|----------------------------|
| Source files     | PascalCase with `.nr`        | `Vector2.nr`              |
| Class names      | Must match filename           | `Vector2 is public class`  |
| Documentation    | Match source name with `.md`  | `docs/scripts/Main.md`     |
| Config files     | lowercase or dotfile          | `neuron.toml`, `.neuronsettings` |
| Directories      | lowercase                     | `src/`, `modules/`, `build/` |

When `enforce_strict_file_naming = true`, the compiler validates that all file and class names follow these conventions.

---

## 5. Module Resolution

When a `module <Name>;` statement is encountered, the compiler searches for the module in the following order:

1. **`src/` directory** â€” Recursively searches for `<Name>.nr`
2. **`modules/` directory** â€” Checks installed package dependencies
3. **Standard library** â€” Built-in modules like `System`, `Math`, `IO`

A file must not import itself â€” the compiler reports a semantic error for self-import attempts.

---

## 6. Build Output

After running `neuron build` or `neuron run`, the `build/` directory contains:

```
build/
â”œâ”€ Main.ll          # LLVM IR (when emit_ir is configured)
â”œâ”€ Main.obj         # Object file
â”œâ”€ Main.exe         # Native executable (platform-dependent extension)
â””â”€ .neuron_cache/   # Tensor kernel cache and build artifacts
    â””â”€ tensor/
```

For NCON execution, containers are placed in a temporary session directory managed by the runtime.

---

## 7. Scaling Guidelines

- **Keep one class per file** â€” This is enforced by the compiler
- **Organize by domain** â€” Group related modules into subdirectories under `src/`
- **Use modules for reuse** â€” Extract shared code into the `modules/` directory
- **Document public APIs** â€” Maintain `docs/scripts/` files for all public source files
- **Limit file size** â€” Stay within `max_lines_per_file` (default: 1000)
- **Limit method size** â€” Stay within `max_lines_per_method` (default: 50)
