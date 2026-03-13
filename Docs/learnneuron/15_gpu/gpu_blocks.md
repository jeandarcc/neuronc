# GPU Blocks

The `gpu { }` block offloads tensor operations to the GPU, with automatic CPU fallback.

---

## Basic Syntax

```npp
gpu {
    result is a + b;
    product is a * b;
}
```

Everything inside the block runs on the GPU when available.

---

## CPU Fallback

If no GPU is available, the same code runs on the CPU â€” no code changes needed.

---

## Real Example

From `GpuBlockTensorOps.nr`:

```npp
Init method() {
    a is Tensor<float>.Random(2, 2);
    b is Tensor<float>.Random(2, 2);
    c is Tensor<float>.Random(2, 2);

    cpuSum is a + b;
    cpuMul is a * b;

    gpu {
        gpuSum is a + b;
        gpuMul is a * b;
        gpuFmaLike is (a * b) + c;
    }

    Print("gpu block prefers GPU and falls back to CPU.");
}
```

---

## Restrictions

- `gpu { }` blocks are **only allowed inside method bodies**
- Operations must be tensor-based (element-wise, matmul)
- No control flow (if/while/for) inside gpu blocks

---

## Next Steps

- [GPU Selectors](gpu_selectors.md) â€” Device preferences
- [Backends](backends.md) â€” GPU backend selection
