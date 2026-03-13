# Tensor Core (`runtime/src/tensor.c` & `runtime/src/tensor/`)

The Tensor subsystem is the mathematical heart of the Neuron runtime. It handles
N-dimensional array representation, memory layouts, strides, broadcasting, and
high-performance mathematical operations.

## Architecture

Unlike some submodules, the tensor logic is primarily housed in a massive single
file (`runtime/src/tensor.c` - ~94k bytes) to allow aggressive C-level cross-inlining
by the GCC/Clang compilers.

| File/Directory | Purpose |
|----------------|---------|
| `tensor.c` | Allocation, reference counting, shape/stride computation, layout transformations (transpose, slice, view), and top-level mathematical operations (add, mul, matmul, relu). Handles broadcasting and type dispatch to SIMD optimized loops. |
| `tensor/` | Contains specialized sub-components, SIMD micro-kernels, threading pool configurations, OOM handlers, and allocator overrides. |

## Data Layout & Contiguity

Tensors in Neuron are not always contiguous in memory. A tensor is defined by:
- A `data` pointer
- A shape array (e.g. `[128, 64]`)
- A stride array (e.g. `[64, 1]`)

If `stride[i] == shape[i+1] * stride[i+1]`, the tensor is contiguous. `tensor.c`
contains fast-paths for contiguous tensors, and slow-paths (using N-dimensional
index iterators) for non-contiguous views (like a sliced or transposed tensor).

## Extensibility

When adding a new mathematical operation:
1. Define the public signature in `include/neuronc/runtime/tensor.h` (or similar).
2. Add the broadcasting/dispatch logic in `tensor.c`.
3. If it requires specialized AVX2/NEON intrinsics, route it to `tensor/`.
