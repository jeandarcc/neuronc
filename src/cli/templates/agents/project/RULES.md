# Project Rules

This document defines the project-specific constraints and workflows that apply to this Neuron project.

---

## 1. Source Organization

- All source code resides in the `src/` directory.
- Each `.nr` file contains exactly **one class**.
- The class name must **match the filename** (case-sensitive).
- The program entry point is the `Init` method in `src/Main.nr`.

---

## 2. Build and Run

| Command         | Action                                              |
|-----------------|-----------------------------------------------------|
| `neuron build`  | Compile the project to a native binary via LLVM     |
| `neuron run`    | Build and execute via NCON bytecode container       |
| `neuron ncon watch` | Continuous build with file watching and hot-reload |

### Hot Reload

When `ncon.hot_reload = true` in `neuron.toml`, source changes that are structurally compatible (e.g., method body edits) are patched live without restarting the program. Structural changes (e.g., new fields, changed types) trigger a full restart.

---

## 3. Dependencies

- Add dependencies with `neuron add <package>`.
- Remove with `neuron remove <package>`.
- Update all with `neuron update`.
- Dependencies are declared in the `[dependencies]` section of `neuron.toml`.
- Installed packages are stored in the `modules/` directory.

---

## 4. Documentation

- Every source file requires a corresponding markdown file in `docs/scripts/`.
- Documentation files must have at least 5 non-empty lines (configurable via `require_script_docs_min_lines`).
- Test files matching `require_script_docs_exclude` patterns are exempt.
- Public methods require documentation comments when `require_public_method_docs = true`.

---

## 5. Testing

- Place automated tests in `tests/auto/`.
- Place unit tests in `tests/unit/`.
- Tests run during `neuron release` and must complete within `max_auto_test_duration_ms` (default: 5000ms).

---

## 6. Release Workflow

The `neuron release` command performs:

1. **Build** — Full project compilation
2. **Test** — Automated test execution
3. **Package** — Creation of distributable bundle

---

## 7. Code Quality

The following rules are enforced by `.neuronsettings` (see `agents/language/Details/RULES.md`):

- Method names must start with an uppercase letter (PascalCase)
- Variable names must start with a lowercase letter (camelCase)
- Constants must use UPPER_CASE naming
- Maximum nesting depth: 3 levels
- Maximum lines per method: 50
- Maximum lines per file: 1000

---

## 8. Version Control

The `.gitignore` excludes:

- `build/` — Compiled output
- `agents/` — Generated documentation (regenerated from templates)

All source code, configuration files, tests, and documentation should be committed.
