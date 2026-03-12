# Tensor Operations

Neuron++ supports element-wise arithmetic and matrix multiplication on tensors.

---

## Element-Wise Operations

```npp
t3 is t1 + t2;    // addition
t4 is t1 - t2;    // subtraction
t5 is t1 * t2;    // element-wise multiplication
t6 is t1 / t2;    // element-wise division
```

---

## Matrix Multiplication

The `@` operator performs matrix multiplication:

```npp
t7 is t1 @ t2;    // matrix multiply
```

---

## Fused Operations (Kernel Fusion)

Multiple tensor operations are automatically fused into a single GPU/CPU kernel to minimize memory bandwidth usage:

```npp
result is (a * b) + c;    // Single kernel: out[i] = (a[i] * b[i]) + c[i]
```

### Optimization Techniques
- **Cache Blocking:** Tiling matrices to fit in L1/L2 cache.
- **SIMD Vectorization:** Using AVX/SSE for CPU speedup.
- **Loop Tiling:** Restructuring nested loops for spatial locality.

---

## Real Example

From `TensorTest.npp`:

```npp
module Tensor;

TensorDemo method(t1 as Tensor<float>, t2 as Tensor<float>) {
    t3 is t1 + t2;
    t4 is t1 - t2;
    t5 is t1 * t2;
    t6 is t1 / t2;
    t7 is t1 @ t2;
    Print(t3);
    Print(t7);
}
```

---

## Next Steps

- [Broadcasting](broadcasting.md) — Shape rules
- [Neural Ops](neural_ops.md) — Activation functions, BLAS
