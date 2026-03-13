# Tensor Creation

Tensors are multi-dimensional arrays â€” the core data structure for numerical computing in Neuron.

---

## Importing the Tensor Module

```npp
module Tensor;
```

---

## Creating Tensors

```npp
// Random 2Ã—2 float tensor
a is Tensor<float>.Random(2, 2);

// All zeros
b is Tensor<float>.Zeros(3, 3);

// All ones
c is Tensor<float>.Ones(4, 4);
```

---

## Real Example

From `GpuBlockTensorOps.nr`:

```npp
a is Tensor<float>.Random(2, 2);
b is Tensor<float>.Random(2, 2);
c is Tensor<float>.Random(2, 2);
```

---

## Next Steps

- [Operations](operations.md) â€” Arithmetic on tensors
