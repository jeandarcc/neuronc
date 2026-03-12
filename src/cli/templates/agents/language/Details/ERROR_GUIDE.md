# Neuron++ Error Guide

This document catalogs the most common compiler diagnostics emitted by the Neuron++ toolchain, explains their root causes, and provides actionable resolution steps.

---

## E001 — Multiple Classes in Module

**Diagnostic:**
```
ERROR: Multiple classes defined in module.
Each .npp file may contain only one class.
```

**Cause:** A single `.npp` file contains more than one `class` declaration. Neuron++ enforces a strict one-class-per-file policy.

**Fix:** Split each class into its own `.npp` file. The filename must match the class name.

```
❌  Shapes.npp  →  Circle is public class { }  +  Square is public class { }
✅  Circle.npp  →  Circle is public class { }
✅  Square.npp  →  Square is public class { }
```

---

## E002 — Class Name / Filename Mismatch

**Diagnostic:**
```
ERROR: Class name must match module filename.
Expected: <Filename>
Found: <ClassName>
```

**Cause:** The class declared inside a file does not match the file's base name.

**Fix:** Rename either the file or the class so they match exactly (case-sensitive).

```
❌  Vector2.npp  →  Vec2 is public class { }
✅  Vector2.npp  →  Vector2 is public class { }
```

---

## E003 — Self-Import

**Diagnostic:**
```
ERROR: Module cannot import itself.
```

**Cause:** A file contains a `module` statement that references its own name. For example, `Box.npp` contains `module Box;`.

**Fix:** Remove the self-referencing `module` statement. A file's own declarations are automatically in scope.

---

## E004 — Method Name Violation

**Diagnostic:**
```
ERROR: Method name must start with an uppercase letter.
```

**Cause:** When `require_method_uppercase_start = true` is set in `.neuronsettings`, all method names (except `constructor`) must begin with a capital letter.

**Fix:** Rename the method to start with an uppercase letter.

```
❌  calculateSum is method(a as int, b as int) as int { ... };
✅  CalculateSum is method(a as int, b as int) as int { ... };
```

---

## E005 — Variable Name Violation

**Diagnostic:**
```
ERROR: Invalid variable name.
```

**Cause:** The variable name does not follow naming rules: must start with a lowercase letter (or `_` followed by a lowercase letter), no mid-name underscores, and only alphanumeric characters.

**Fix:**

```
❌  TestObject        // starts with uppercase
❌  test_object       // underscore in the middle
❌  __cache           // double underscore
✅  testObject
✅  _testObject
```

---

## E006 — Missing Access Modifier

**Diagnostic:**
```
ERROR: Class/property requires explicit visibility modifier.
```

**Cause:** The `.neuronsettings` file requires explicit `public` or `private` on all class and/or property declarations.

**Fix:** Add `public` or `private` before the declaration.

```
❌  Player is class { ... }
✅  Player is public class { ... }

❌  health is int;
✅  health is private int;
```

---

## E007 — Script Documentation Missing or Too Short

**Diagnostic:**
```
ERROR: Script documentation '<path>' is too short (N non-empty lines, minimum M).
```

**Cause:** The corresponding `docs/scripts/<ScriptName>.md` file either does not exist or does not meet the minimum line count specified by `require_script_docs_min_lines`.

**Fix:** Create or extend the documentation file so it has at least the required number of non-empty lines. Include a title, purpose description, and usage details.

```markdown
# Main Script

This document describes `src/Main.npp`.

## Purpose
Entry point for the Neuron++ project. Runs the `Init` method on startup.

## Usage
The `Init` method is automatically invoked by the runtime when the project
is executed via `neuron run`. No manual call is required.
```

---

## E008 — File Too Long

**Diagnostic:**
```
ERROR: File exceeds maximum line count (N > max_lines_per_file).
```

**Cause:** The `.npp` file exceeds the `max_lines_per_file` setting (default: 1000).

**Fix:** Refactor the file by extracting logic into separate modules or helper methods.

---

## E009 — Method Too Long

**Diagnostic:**
```
ERROR: Method body exceeds maximum line count (N > max_lines_per_method).
```

**Cause:** A single method body exceeds the `max_lines_per_method` setting (default: 50).

**Fix:** Break the method into smaller, focused helper methods.

---

## E010 — Nesting Too Deep

**Diagnostic:**
```
ERROR: Block nesting exceeds maximum depth (N > max_nesting_depth).
```

**Cause:** Control structures are nested beyond the `max_nesting_depth` limit (default: 3).

**Fix:** Use early returns, guard clauses, or extract nested logic into separate methods.

```npp
// ❌ Too deeply nested
if(a) {
    if(b) {
        if(c) {
            if(d) { ... }   // depth 4 — exceeds limit
        }
    }
}

// ✅ Flattened with early returns
if(!a) { return; }
if(!b) { return; }
if(!c) { return; }
Process(d);
```

---

## E011 — Undefined Variable

**Diagnostic:**
```
ERROR: Use of undeclared identifier '<name>'.
```

**Cause:** A variable is referenced before being declared in the current scope.

**Fix:** Ensure the variable is declared with `is` before use, or import the module that defines it.

---

## E012 — Type Mismatch

**Diagnostic:**
```
ERROR: Type mismatch — expected '<TypeA>', found '<TypeB>'.
```

**Cause:** An operation or assignment involves incompatible types.

**Fix:** Use explicit type annotation with `as`, or ensure the operand types are compatible.

```npp
x is 10 as int;
y is 3.14 as float;
// z is x + y;          // potential type mismatch
z is (x as float) + y;  // explicit cast
```

---

## E013 — Invalid `gpu` Block Usage

**Diagnostic:**
```
WARNING: GPU backend unavailable — falling back to CPU.
```

**Cause:** A `gpu { ... }` block was used, but no GPU backend (CUDA or Vulkan) is available on the target system.

**Resolution:** This is a warning, not an error. The program will still execute correctly using the CPU fallback path. To force a specific backend, set the environment variable:

```
NEURON_GPU_FORCE_BACKEND=auto|cpu|vulkan|cuda
```

---

## E014 — Constant Naming Violation

**Diagnostic:**
```
ERROR: Constant name must be UPPER_CASE.
```

**Cause:** When `require_const_uppercase = true`, compile-time constants must use uppercase naming.

**Fix:**

```
❌  maxSize is 1024;
✅  MAX_SIZE is 1024;
```

---

## General Debugging Tips

1. **Read the full error message** — Neuron++ diagnostics include the file, line number, and expected vs. found values.
2. **Check `.neuronsettings`** — Many errors are caused by project-level rule configurations.
3. **One class per file** — This is the most common mistake for newcomers from C++ or Java.
4. **Alias vs. copy** — If two variables unexpectedly share state, you may need `another` for an independent copy.
5. **Module self-import** — A file cannot import itself; its own symbols are already in scope.
