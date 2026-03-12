# Neuron++ Language Rules

This document defines the core language rules enforced by the Neuron++ compiler and the `.neuronsettings` configuration file. All contributors and automated agents **must** follow these rules when writing or generating Neuron++ code.

---

## 1. One Class Per File

Every `.npp` source file may contain **at most one class definition**. The class name must exactly match the filename (without extension).

```
✅  Vector2.npp  →  Vector2 is public class { ... }
❌  Vector2.npp  →  Vec2 is public class { ... }      // name mismatch
❌  Shapes.npp   →  Circle is public class { ... }
                    Square is public class { ... }     // multiple classes
```

**Compiler error:**
```
ERROR: Class name must match module filename. Expected: Vector2, Found: Vec2
ERROR: Multiple classes defined in module. Each .npp file may contain only one class.
```

---

## 2. Statement Termination

Every statement must end with a semicolon (`;`):

```npp
x is 10;
y is another x;
Print(y);
```

---

## 3. Entry Point

The program entry point is the `Init` method defined in `src/Main.npp`. When a project is run, the compiler locates `Main.npp`, finds `Init`, and executes it.

```npp
// src/Main.npp
Init is method() {
    Print("Program started");
};
```

---

## 4. Module Import Rules

- Use `module <Name>;` to import external modules.
- A file **must not** import itself. For example, `Box.npp` must not contain `module Box;`.
- The compiler reports a semantic error for self-import attempts.

```npp
// In Main.npp — valid
module Vector2;
module Math;
```

---

## 5. Access Modifiers

Classes, methods, and fields support explicit visibility:

| Modifier  | Scope                            |
|-----------|----------------------------------|
| `public`  | Accessible from any module       |
| `private` | Accessible only within the class |

When `.neuronsettings` has `require_class_explicit_visibility = true`, every class declaration must include an access modifier. The same applies to properties when `require_property_explicit_visibility = true`.

---

## 6. Method Naming

Method names are validated with the following rules:

- Must contain only letters and digits.
- Must not start with a digit.
- `constructor` is a reserved special name for class constructors.
- When `require_method_uppercase_start = true` in `.neuronsettings`, method names must start with an uppercase letter.

```
✅  CalculateLength, Init, ProcessData
❌  calculateLength      (if uppercase start required)
❌  2ndPass              (starts with digit)
```

---

## 7. Variable Naming

Variable names follow these conventions:

- Must start with a lowercase letter, or with `_` followed by a lowercase letter.
- `_` is only allowed as the first character (no mid-name underscores).
- Remaining characters must be letters or digits.

```
✅  testObject, _testObject, counter, xPos
❌  TestObject         (starts with uppercase)
❌  test_Object        (underscore in the middle)
❌  __cache            (double underscore)
```

---

## 8. Constant Naming

When `require_const_uppercase = true` in `.neuronsettings`, compile-time constant identifiers must use UPPER_CASE naming.

```npp
MAX_SIZE is 1024;
PI is 3.14159;
```

---

## 9. File Length Limits

The following limits can be configured in `.neuronsettings`:

| Setting                   | Default | Description                            |
|---------------------------|---------|----------------------------------------|
| `max_lines_per_file`      | `1000`  | Maximum total lines per `.npp` file    |
| `max_lines_per_method`    | `50`    | Maximum lines per method body          |
| `max_lines_per_block_statement` | `20` | Maximum lines per control block     |
| `max_nesting_depth`       | `3`     | Maximum nesting depth of blocks        |
| `min_method_name_length`  | `4`     | Minimum character length for names     |

---

## 10. Documentation Requirements

When `require_script_docs = true`:

- Every script file must have a corresponding documentation file under `docs/scripts/<ScriptName>.md`.
- Documentation files must have at least `require_script_docs_min_lines` non-empty lines (default: `5`).
- Files matching patterns in `require_script_docs_exclude` (e.g., `Test*`) are exempt.
- When `require_public_method_docs = true`, every public method must have a documentation comment.

---

## 11. Alias vs. Copy Semantics

Assignments create aliases (shared memory) by default. Use `another` to create an independent copy:

```npp
x is 10;
y is x;            // alias — y shares memory with x
z is another x;    // copy — z has its own memory
```

This behavior is fundamental to Neuron++. Misunderstanding it is a common source of bugs.

---

## 12. Type System

- Explicit types are declared with the `as` keyword: `x is 10 as int;`
- When no type is specified, the compiler uses type inference.
- Generic types use angle-bracket syntax: `Array<int>`, `Tensor<float>`.
- Type constraints restrict generic parameters: `<T:Numeric>`.

---

## 13. Error Handling

- Use `try / catch / finally` blocks for exception handling.
- Custom error types are defined as classes: `DivideByZeroError is public class { }`.
- `throw` raises an exception; `catch(error)` catches any exception type.

---

## 14. `.neuronsettings` Reference

The `.neuronsettings` file in the project root configures all compiler rules:

```ini
max_classes_per_file = 1
max_lines_per_file = 1000
require_method_uppercase_start = true
enforce_strict_file_naming = true
forbid_root_scripts = true
max_lines_per_method = 50
max_lines_per_block_statement = 20
min_method_name_length = 4
require_class_explicit_visibility = true
require_property_explicit_visibility = true
require_const_uppercase = true
max_nesting_depth = 3
require_script_docs = true
require_script_docs_exclude = ["Test*"]
require_script_docs_min_lines = 5
max_auto_test_duration_ms = 5000
require_public_method_docs = true
agent_hints = true
```

Setting a numeric limit to `0` disables that particular rule.
