# Neuron++ Project Structure

This document defines the recommended directory layout for Neuron++ projects and explains the purpose of each directory and configuration file.

---

## 1. Standard Project Layout

When you create a new project with `neuron new <name>`, the following structure is generated:

```
my_project/
│
├─ neuron.toml              # Project configuration and build settings
├─ .neuronsettings          # Source code rules and lint configuration
├─ .gitignore               # Git exclusion rules
│
├─ src/                     # Source code directory
│   └─ Main.npp             # Program entry point (Init method)
│
├─ modules/                 # External module dependencies
│
├─ build/                   # Build output directory (auto-generated)
│
├─ docs/                    # Project documentation
│   └─ scripts/             # Per-script documentation files
│       └─ Main.md          # Documentation for Main.npp
│
├─ tests/                   # Test suites
│   ├─ auto/                # Automated integration tests
│   └─ unit/                # Unit test files
│
└─ agents/                  # Agent and tooling reference documents
    ├─ language/             # Language-level documentation
    │   ├─ LanguageGuide.md  # Complete language overview
    │   └─ Details/          # In-depth topic guides
    │       ├─ RULES.md
    │       ├─ ERROR_GUIDE.md
    │       ├─ NAMING.md
    │       ├─ GPU_SEMANTICS.md
    │       └─ STRUCTURE.md
    └─ project/              # Project-specific documentation
        ├─ ARCHITECTURE.md
        └─ RULES.md
```

---

## 2. Key Directories

### `src/`

Contains all `.npp` source files. The main entry point is always `src/Main.npp`, which must define an `Init` method.

**Organization for larger projects:**

```
src/
├─ Main.npp
├─ Math/
│   ├─ Vector2.npp
│   ├─ Vector3.npp
│   └─ Matrix.npp
├─ AI/
│   ├─ Tensor.npp
│   ├─ Layer.npp
│   └─ Optimizer.npp
└─ Utils/
    └─ Logger.npp
```

Each `.npp` file defines exactly one class, and the class name must match the filename.

### `modules/`

External module dependencies managed by the package manager. Populated by `neuron add <package>`.

### `build/`

Build output directory. Contains compiled binaries, object files, LLVM IR, and NCON containers. This directory is auto-generated and should be added to `.gitignore`.

### `docs/scripts/`

Per-script documentation files. When `require_script_docs = true` in `.neuronsettings`, every source file must have a corresponding `.md` file here (e.g., `docs/scripts/Main.md` for `src/Main.npp`).

### `tests/`

- **`tests/auto/`** — Automated integration tests that run during `neuron release`
- **`tests/unit/`** — Unit test files for individual modules

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
main = "src/Main.npp"
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
| Source files     | PascalCase with `.npp`        | `Vector2.npp`              |
| Class names      | Must match filename           | `Vector2 is public class`  |
| Documentation    | Match source name with `.md`  | `docs/scripts/Main.md`     |
| Config files     | lowercase or dotfile          | `neuron.toml`, `.neuronsettings` |
| Directories      | lowercase                     | `src/`, `modules/`, `build/` |

When `enforce_strict_file_naming = true`, the compiler validates that all file and class names follow these conventions.

---

## 5. Module Resolution

When a `module <Name>;` statement is encountered, the compiler searches for the module in the following order:

1. **`src/` directory** — Recursively searches for `<Name>.npp`
2. **`modules/` directory** — Checks installed package dependencies
3. **Standard library** — Built-in modules like `System`, `Math`, `IO`

A file must not import itself — the compiler reports a semantic error for self-import attempts.

---

## 6. Build Output

After running `neuron build` or `neuron run`, the `build/` directory contains:

```
build/
├─ Main.ll          # LLVM IR (when emit_ir is configured)
├─ Main.obj         # Object file
├─ Main.exe         # Native executable (platform-dependent extension)
└─ .neuron_cache/   # Tensor kernel cache and build artifacts
    └─ tensor/
```

For NCON execution, containers are placed in a temporary session directory managed by the runtime.

---

## 7. Scaling Guidelines

- **Keep one class per file** — This is enforced by the compiler
- **Organize by domain** — Group related modules into subdirectories under `src/`
- **Use modules for reuse** — Extract shared code into the `modules/` directory
- **Document public APIs** — Maintain `docs/scripts/` files for all public source files
- **Limit file size** — Stay within `max_lines_per_file` (default: 1000)
- **Limit method size** — Stay within `max_lines_per_method` (default: 50)
