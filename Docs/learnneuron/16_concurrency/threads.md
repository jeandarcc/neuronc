# Threads

The `thread` keyword spawns concurrent threads of execution.

---

## Syntax

```npp
thread {
    // runs in a separate thread
    HeavyComputation();
}
```

---

## Thread Semantics

- Each `thread` block spawns a new OS-level thread
- Execution continues immediately in the calling code
- Use for CPU-bound parallel work across multiple cores

---

## When to Use

| Scenario | Use |
|----------|-----|
| Independent CPU computations | `thread` |
| Data-parallel loops | `parallel for` |
| GPU tensor operations | `gpu { }` |
| I/O-bound operations | `async` / `await` |

---

## Next Steps

- [Atomics](atomics.md)
