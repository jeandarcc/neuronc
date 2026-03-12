# Structure Rules

Rules that control file-level organization and project structure.

---

## Rules

| Rule | Default | Effect |
|------|---------|--------|
| `max_classes_per_file` | `1` | At most 1 class per `.npp` file |
| `max_lines_per_file` | `1000` | Maximum source file length |
| `forbid_root_scripts` | `true` | `.npp` files cannot exist in the project root |

---

## `max_classes_per_file = 1`

The most fundamental structural rule in Neuron++. Each `.npp` file contains exactly **one class**.

```npp
// ✅ Player.npp — one class
Player public class {
    health private is 100 as int;
}

// ❌ Rejected — two classes in one file
Player public class { }
Enemy public class { }
```

```
neuronc: error: Multiple classes defined in module. Each .npp file may
contain only one class.
```

**Why:** One class per file makes the project navigable. You can find any class by its filename — no grepping, no guessing. This is equally important for humans and AI agents.

---

## `max_lines_per_file = 1000`

No source file can exceed 1000 lines.

```
neuronc: error: File 'src/Engine.npp' exceeds maximum allowed length
(1000 lines). Configure max_lines_per_file in .neuronsettings.
```

**Why:** A file over 1000 lines is a signal that the class has too many responsibilities. Split it.

---

## `forbid_root_scripts = true`

`.npp` files cannot be placed directly in the project root directory. They must live in subdirectories like `src/`, `modules/`, or `tests/`.

```
Error: Scripts cannot live at repository root when
forbid_root_scripts = true: 'MyCode.npp'.
For agents: Move root scripts under folders like src/, modules/, or tests/.
```

**Why:** Prevents messy projects where source files are scattered in the root alongside config files.

---

## Next Steps

- [Naming Rules](naming_rules.md)
