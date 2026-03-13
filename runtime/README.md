# Neuron Runtime (`runtime/`)

This directory contains the high-performance C runtime library for Neuron.
The runtime provides the execution environment, memory management, I/O, platform
abstractions, and the neural/tensor core.

It is compiled ahead-of-time into a static library or object
files, which are then linked into the final executable by `LLVMCodeGen` or loaded
dynamically by the `ncon` JIT engine.

## Directory Layout & Core Files

| File/Directory | Purpose |
|----------------|---------|
| `src/runtime.c` | Library initialization, error handling, thread-local storage setup. |
| `src/modulecpp_runtime.cpp` | The FFI bridge connecting `neuronc` generated code and `ncon` to the pure C runtime. |
| `src/io.c` | File system, streams, and network sockets (backed by `platform/`). |
| `src/nn.c` | High-level neural network operations (layers, activations, loss functions). |
| `src/tensor.c` & `src/tensor/` | Multi-dimensional array tracking, memory layout, and SIMD mathematical micro-kernels. |
| `src/gpu.c`, `src/gpu_cuda.c` | CUDA device management and kernel dispatch logic. Includes raw `cuda_kernels.h`. |
| `src/gpu_vulkan/` | Vulkan device management, SPIR-V pipeline caching, and compute dispatch (`vulkan_common_*.c`). |
| `src/graphics/` | 2D/3D scene graph, asset loading (GLTF/PNG), and the windowing/swapchain backend. |
| `src/platform/` | OS abstractions (Win32, POSIX, Apple, Web). Exposes `SwitchPlatformManager`. |

## Build Notes

The runtime is built using CMake, but it is deeply integrated with the `scripts/build.bat` flow.

> **Warning:** Do not `#include <neuronc/...>` or any LLVM headers in the runtime.
> The runtime is a pure C11 / minimal C++ library capable of being compiled by GCC/Clang
> independently of the compiler pipeline.
