# Parallel For

Execute loop iterations concurrently across CPU threads.

---

## Syntax

```npp
parallel for (i is 0; i < data.Length; i++) {
    data[i] is data[i] * 2;
}
```

---

## How It Works

The `parallel` modifier before `for` distributes iterations across available CPU threads. Each iteration runs independently.

---

## When to Use

- Large data transformations
- Element-wise operations on arrays/tensors
- Independent computations per iteration

---

## Restrictions

- Iterations must be **independent** — no shared mutable state
- Order of execution is not guaranteed

---

## GPU Alternative

For GPU-accelerated parallelism, use `gpu { }` blocks instead:

```npp
gpu {
    result is A + B;    // GPU-parallel tensor operation
}
```

See [GPU Blocks](../15_gpu/gpu_blocks.md).

---

## Next Steps

- [Break / Continue / Return](break_continue_return.md)
