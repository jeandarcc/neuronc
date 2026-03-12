# Neuron++ Naming Conventions

This document defines the official naming standards for all identifiers in Neuron++ projects. Following these conventions ensures consistency, readability, and compatibility with compiler validation rules.

---

## 1. Methods

| Rule | Description |
|------|-------------|
| **Casing** | PascalCase (first letter uppercase, each subsequent word capitalized) |
| **Characters** | Letters and digits only — no underscores or special characters |
| **Start** | Must begin with an uppercase letter (when `require_method_uppercase_start = true`) |
| **Reserved** | `constructor` is the special constructor name and is exempt from PascalCase |

### Examples

```
✅  CalculateLength
✅  Init
✅  ProcessBatch
✅  GetX
✅  constructor

❌  calculateLength     (lowercase start)
❌  calculate_length    (underscore)
❌  2ndPass             (starts with digit)
❌  get-value           (hyphen)
```

### Rationale

PascalCase method names visually distinguish method calls from variable access, improving code readability at a glance.

---

## 2. Variables

| Rule | Description |
|------|-------------|
| **Casing** | camelCase (first letter lowercase, each subsequent word capitalized) |
| **Characters** | Letters and digits only, except an optional leading `_` |
| **Start** | Must begin with a lowercase letter, or `_` followed by a lowercase letter |
| **Underscores** | Only allowed as the very first character — never in the middle |

### Examples

```
✅  playerHealth
✅  xPosition
✅  itemCount
✅  _internalState

❌  PlayerHealth        (uppercase start — this is a method name pattern)
❌  player_health       (mid-name underscore)
❌  __cache             (double underscore)
❌  3dPosition          (starts with digit)
```

### Rationale

camelCase is the standard for local and instance variables across many modern languages. The leading `_` prefix is reserved for internal/private-use variables.

---

## 3. Classes

| Rule | Description |
|------|-------------|
| **Casing** | PascalCase |
| **File Rule** | Class name must exactly match the filename (without `.npp` extension) |
| **Visibility** | Must include `public` or `private` when explicit visibility is required |

### Examples

```
✅  Vector2.npp       →  Vector2 is public class { ... }
✅  NeuralNetwork.npp →  NeuralNetwork is public class { ... }
✅  DataLoader.npp    →  DataLoader is public class { ... }

❌  vector2.npp       →  vector2 is public class { ... }    (lowercase filename)
❌  Vector2.npp       →  Vec2 is public class { ... }       (name mismatch)
```

---

## 4. Constants

| Rule | Description |
|------|-------------|
| **Casing** | UPPER_CASE with underscores separating words |
| **Enforcement** | Active when `require_const_uppercase = true` in `.neuronsettings` |

### Examples

```npp
MAX_BUFFER_SIZE is 4096;
PI is 3.14159265;
DEFAULT_LEARNING_RATE is 0.001;
```

---

## 5. Modules

| Rule | Description |
|------|-------------|
| **Casing** | PascalCase (matches the class/file it refers to) |
| **Statement** | `module <Name>;` at the top of the file |

### Examples

```npp
module Vector2;
module NeuralNetwork;
module System;
module Math;
```

---

## 6. Generic Type Parameters

| Rule | Description |
|------|-------------|
| **Casing** | Single uppercase letter or short PascalCase name |
| **Convention** | `T` for general type, `K`/`V` for key/value, descriptive names for clarity |

### Examples

```npp
Array<T> is public class { ... }
HashMap<K, V> is public class { ... }
Tensor<ElementType> is public class { ... }
Add<T:Numeric> is method(T a, T b) as T { ... };
```

---

## 7. Custom Error Types

| Rule | Description |
|------|-------------|
| **Casing** | PascalCase with `Error` suffix |

### Examples

```npp
DivideByZeroError is public class { }
FileNotFoundError is public class { }
IndexOutOfRangeError is public class { }
```

---

## 8. Project Files and Directories

| Item | Convention | Example |
|------|-----------|---------|
| Source files | PascalCase `.npp` | `Vector2.npp`, `DataLoader.npp` |
| Source directory | lowercase | `src/` |
| Module directory | lowercase | `modules/` |
| Build directory | lowercase | `build/` |
| Test files | PascalCase with `Test` prefix | `TestVector2.npp` |
| Documentation | PascalCase `.md` matching script | `docs/scripts/Main.md` |

---

## 9. Summary Table

| Identifier Type     | Casing       | Example               |
|---------------------|--------------|-----------------------|
| Method              | PascalCase   | `CalculateLength`     |
| Variable            | camelCase    | `playerHealth`        |
| Class               | PascalCase   | `NeuralNetwork`       |
| Constant            | UPPER_CASE   | `MAX_BUFFER_SIZE`     |
| Module import       | PascalCase   | `module Vector2;`     |
| Generic parameter   | T / PascalCase | `<T>`, `<ElementType>` |
| Error class         | PascalCase + `Error` | `FileNotFoundError` |
| File name           | PascalCase   | `Vector2.npp`         |

---

## 10. Enforcement

These naming rules are enforced at two levels:

1. **Compiler validation** — The Neuron++ compiler checks method names, variable names, and class-filename matches at compile time.
2. **`.neuronsettings` configuration** — Project-level settings toggle specific rules such as uppercase method starts, constant naming, and minimum name lengths.

Violations produce clear compiler diagnostics with expected vs. found values. See `ERROR_GUIDE.md` for specific error codes and resolution steps.
