# Importing Modules

The `module` keyword imports external code into the current file.

---

## Syntax

```npp
module Math;
module Vector2;
module System;
```

Place `module` statements at the top of the file, before any code.

---

## Module Resolution Order

1. **`src/` directory** — Searches recursively for `<Name>.npp`
2. **`modules/` directory** — Installed package dependencies
3. **Standard library** — Built-in modules (`System`, `Math`, etc.)

---

## Using Imported Modules

```npp
module Math;

Init method() {
    result is Math.sqrt(16.0);
    Print(result);    // 4.0
}
```

---

## Self-Import Prohibition

A file cannot import itself — the compiler reports an error.

---

## Real Example

From `TensorTest.npp`:

```npp
module System;
module Tensor;

Init method() {
    t1 is CreateTensor();
    t2 is CreateTensor();
    Print("Done.");
}
```

---

## Next Steps

- [ModuleCpp](modulecpp.md) — C++ interop
- [Standard Library](standard_library.md) — Built-in modules
