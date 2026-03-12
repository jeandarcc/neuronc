# Unsafe Blocks

The `unsafe` keyword allows low-level operations that bypass Neuron++'s safety guarantees.

---

## Syntax

```npp
unsafe {
    // raw pointer arithmetic
    // manual memory management
    // platform-specific operations
}
```

---

## What Is Allowed in Unsafe?

- Raw pointer manipulation beyond `address of` / `value of`
- Direct memory access
- Platform-specific intrinsics

---

## Restrictions

- `unsafe` blocks are **rejected** in NCON sandbox mode
- Must be used sparingly — code inside is not safety-checked

---

## When to Use

- Interfacing with hardware at the lowest level
- Performance-critical hot paths where safety checks are too expensive
- FFI compatibility with C/C++ code

---

## Next Steps

- [Extern](extern.md) — Foreign function interface
