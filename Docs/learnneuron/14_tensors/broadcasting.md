# Broadcasting & Slicing

Rules for operating on tensors of different shapes, and extracting sub-tensors.

---

## Broadcasting

When performing operations between tensors of different shapes, Neuron automatically broadcasts the smaller tensor:

```npp
a is Tensor<float>.Random(3, 3);    // 3Ã—3
b is Tensor<float>.Random(1, 3);    // 1Ã—3

c is a + b;    // b is broadcast to 3Ã—3
```

### Rules

1. Dimensions are compared from right to left
2. A dimension of size **1** is stretched to match the other
3. Missing dimensions are treated as size 1

---

## Slicing

Extract sub-tensors using bracket notation:

```npp
data is Tensor<float>.Random(10, 10);
row is data[0];           // first row
slice is data[0..5];      // first 5 rows
```

---

## Next Steps

- [Neural Ops](neural_ops.md) â€” Activation functions and BLAS operations
