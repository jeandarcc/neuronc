# Alias Semantics

In Neuron, the default `is` assignment creates an **alias** â€” both names share the same underlying memory.

---

## Default: Alias

```npp
x is 10;
y is x;       // alias â€” y and x share memory
y is 20;
Print(x);     // 20 (x changed too!)
```

---

## The Four Memory Primitives

| Keyword | Behavior | C++ Equivalent |
|---------|----------|----------------|
| `is` | Alias (shared memory) | `T& y = x;` |
| `another` | Deep copy (independent) | `T y = x;` |
| `move` | Ownership transfer | `T y = std::move(x);` |
| `address of` / `value of` | Pointer access | `&x` / `*p` |

---

## Why Alias-by-Default?

- No unnecessary copies â†’ **faster by default**
- Explicit `another` when you need a copy â†’ **clear intent**
- Matches the language philosophy of performance-first design

---

## Next Steps

- [Pointers](pointers.md) â€” `address of` and `value of`
- [Ownership](ownership.md) â€” Lifetime model
