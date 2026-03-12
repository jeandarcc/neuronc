# Atomics

The `atomic` keyword declares thread-safe variables with atomic read/write guarantees.

---

## Syntax

```npp
atomic counter is 0;
```

---

## How It Works

Atomic variables use hardware-level atomic instructions to ensure that reads and writes are:
- **Indivisible** — no partial updates
- **Visible** — changes immediately visible to all threads

---

## Usage with Threads

```npp
atomic counter is 0;

thread {
    counter++;    // thread-safe increment
}

thread {
    counter++;    // thread-safe increment
}
```

---

## When to Use

Use `atomic` when multiple threads access the same variable:

```npp
atomic total is 0;

parallel for (i is 0; i < 1000; i++) {
    atomic total is total + data[i];
}
```

---

## Next Steps

- [NCON Containers](../17_ncon/overview.md) — Portable bytecode
