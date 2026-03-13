# Constants

Neuron provides `const` and `constexpr` for declaring immutable values.

---

## `const` — Runtime Constant

A `const` binding cannot be reassigned after initialization:

```npp
const MAX_HEALTH is 100;
const PI is 3.14159;
```

Attempting to reassign a constant produces a compiler error.

---

## `constexpr` — Compile-Time Constant

A `constexpr` value is evaluated entirely at compile time:

```npp
constexpr ComputeMax method() as int {
    return 255;
}
```

`constexpr` methods must produce a result computable at compile time with no side effects.

---

## Naming Convention

Constants must use `UPPER_CASE` naming when `require_const_uppercase = true` in `.neuronsettings`:

```npp
const MAX_SPEED is 100;       // ✅ correct
const maxSpeed is 100;        // ❌ compiler warning/error
```

---

## `const` vs `constexpr`

| Feature | `const` | `constexpr` |
|---------|---------|-------------|
| Evaluated | Runtime | Compile time |
| Reassignment | ❌ Not allowed | ❌ Not allowed |
| Used in methods | ✅ Yes | ✅ Yes (pure methods only) |
| Can call functions | ✅ Any | Only `constexpr` functions |

---

## Next Steps

- [Operators](../03_operators/arithmetic.md) — Arithmetic and comparison operators
