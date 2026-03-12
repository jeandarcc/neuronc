# Type Constraints

Limit generic type parameters to types that satisfy specific requirements.

---

## Syntax

```npp
Add method<T:Numeric>(a as T, b as T) as T {
    return a + b;
}
```

The constraint `T:Numeric` means `T` must be a numeric type (int, float, etc.).

---

## Why Constraints?

Without constraints, any type could be passed — including types that don't support the required operations:

```npp
// Without constraint — could fail at compile time
Add method<T>(a as T, b as T) as T {
    return a + b;    // ❌ what if T is string?
}

// With constraint — guaranteed to work
Add method<T:Numeric>(a as T, b as T) as T {
    return a + b;    // ✅ Numeric types support +
}
```

---

## Next Steps

- [Tensors](../14_tensors/creation.md) — Multi-dimensional data
