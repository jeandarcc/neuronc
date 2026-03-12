# Scope Batching

GPU blocks batch multiple operations into single dispatch calls for efficiency.

---

## Automatic Batching

All operations inside a `gpu { }` block are collected and submitted as a batch:

```npp
gpu {
    a is X + Y;
    b is X * Y;
    c is (X * Y) + Z;
}
// All three operations dispatched in a single GPU submission
```

---

## Loop Batching

Operations inside loops within gpu blocks are also batched:

```npp
gpu {
    for (i is 0; i < 100; i++) {
        result[i] is data[i] * scale;
    }
}
// All 100 iterations batched into one GPU kernel
```

---

## Benefits

- Reduced GPU driver overhead
- Better memory coalescence
- Automatic operation fusion (e.g., multiply-add → FMA)

---

## Next Steps

- [Performance](performance.md) — Optimization tips
