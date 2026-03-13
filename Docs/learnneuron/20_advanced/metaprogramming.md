# Metaprogramming

Neuron provides `macro`, `typeof`, and `static_assert` for compile-time code generation and validation.

---

## `typeof`

Get the type of an expression at compile time:

```npp
x is 42;
t is typeof(x);    // "int"
```

---

## `static_assert`

Assert a condition at compile time â€” compilation fails if the condition is false:

```npp
static_assert(sizeof(int) == 4, "int must be 4 bytes");
```

With a message:

```npp
static_assert(MAX_SIZE > 0, "MAX_SIZE must be positive");
```

---

## `macro`

Macros generate code at compile time:

```npp
macro DebugPrint(expr) {
    Print("Debug: " + typeof(expr) + " = " + expr);
}
```

---

## When to Use

| Feature | Use Case |
|---------|----------|
| `typeof` | Type introspection, generic helpers |
| `static_assert` | Compile-time validation, API contracts |
| `macro` | Code generation, boilerplate reduction |

---

## Next Steps

- [NIR](nir.md) â€” Neuron Intermediate Representation
