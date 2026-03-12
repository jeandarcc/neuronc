# Neural Operations

Specialized tensor operations for neural network workloads.

---

## Fused Multiply-Add (FMA)

The compiler automatically fuses `a * b + c` into a single FMA operation:

```npp
result is (a * b) + c;    // fused to FMA for performance
```

---

## BLAS Operations

Matrix multiplication uses optimized BLAS routines when available:

```npp
output is weights @ input;    // calls BLAS gemm
```

---

## Activation Functions

Apply activation functions from the NN module:

```npp
module NN;

activated is NN.ReLU(preactivation);
softmaxed is NN.Softmax(logits);
```

---

## GPU Acceleration

Wrap tensor operations in `gpu { }` for GPU dispatch:

```npp
gpu {
    result is A @ B;        // GPU matrix multiply
    activated is NN.ReLU(result);
}
```

---

## Next Steps

- [GPU Blocks](../15_gpu/gpu_blocks.md) — GPU execution
