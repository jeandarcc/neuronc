# Neuron++ GPU Semantics

This document describes the GPU execution model in Neuron++, including block semantics, backend selection, kernel dispatch, and performance tuning options.

---

## 1. Overview

Neuron++ provides first-class GPU support through the `gpu { ... }` block construct. Code inside a `gpu` block is eligible for GPU-accelerated execution, with automatic fallback to CPU when no compatible GPU backend is available.

```npp
gpu {
    C is A @ B;       // matrix multiplication — GPU-preferred
    D is C + bias;    // element-wise addition — GPU-preferred
}
```

The design philosophy is **GPU-prefer with silent CPU fallback**: the programmer expresses intent for GPU execution, and the runtime transparently handles device selection.

---

## 2. Backend Priority

When a `gpu { ... }` block is entered, the runtime selects a backend using the following priority:

```
CUDA  >  Vulkan  >  CPU (fallback)
```

Both CUDA and Vulkan backends are enabled by default in Phase-2B. If neither is available, execution falls back to the CPU path without error — only a diagnostic warning is emitted.

### Overriding Backend Selection

Use the `NEURON_GPU_FORCE_BACKEND` environment variable:

| Value    | Behavior                                      |
|----------|-----------------------------------------------|
| `auto`   | Default priority: CUDA > Vulkan > CPU         |
| `cuda`   | Force CUDA; error if unavailable              |
| `vulkan` | Force Vulkan; error if unavailable            |
| `cpu`    | Force CPU execution (no GPU dispatch)         |

```bash
# Force Vulkan backend
set NEURON_GPU_FORCE_BACKEND=vulkan
neuron run
```

---

## 3. GPU Block Semantics

### Scope and Batching

The compiler lowers `gpu { ... }` blocks by emitting `neuron_gpu_scope_begin` / `neuron_gpu_scope_end` calls. All operations within the block are batched into a single GPU scope submit, which means:

- Loops inside a `gpu` block naturally batch into one scope.
- Multiple operations are dispatched together, reducing kernel launch overhead.

```npp
gpu {
    for(i is 0; i < batchCount; i++) {
        results[i] is Process(inputs[i]);   // all iterations in one GPU scope
    }
}
```

### Default Behavior Outside `gpu` Blocks

Outside `gpu { ... }`, tensor operations like `@` (matmul), `matmul_add`, and `linear_fused` use the default CPU-oriented (`AUTO`) dispatch policy. GPU dispatch only activates inside explicit `gpu` blocks.

---

## 4. Supported Operations

### Matrix Multiplication and Fused Operations

Inside `gpu { ... }`, the following operations support GPU dispatch:

| Operation       | Syntax          | Notes                                  |
|-----------------|-----------------|----------------------------------------|
| Matrix multiply | `C is A @ B;`   | Dense 2D row-major `f32` tensors       |
| MatMul + Add    | `Y is X @ W + b;` | Fused matmul-add                     |
| Linear fused    | `linear_fused`  | Matmul with fused activation           |

### Supported Data Format

GPU dispatch currently supports:
- **Element type:** `f32` (32-bit float)
- **Layout:** 2D row-major tensors
- **Input variants:** Dense and packed-B inputs

### Fused Activation Functions

GPU dispatch supports fused activation for the following functions:

| Activation | Description                 |
|------------|-----------------------------|
| `NONE`     | No activation (passthrough) |
| `RELU`     | Rectified Linear Unit       |
| `GELU`     | Gaussian Error Linear Unit  |

```npp
gpu {
    Y is ReLU(X @ W + b);   // fused matmul + bias + ReLU on GPU
}
```

---

## 5. Vulkan Backend Details

The Vulkan backend uses device-local storage buffers with upload/readback staging for data transfer between host and device.

### Memory Transfer

- **Upload:** Host data is copied to a staging buffer, then transferred to device-local storage.
- **Readback:** Results are copied from device-local storage to a staging buffer, then read back to the host.

### Readback Optimization

By default, all tensors modified within a `gpu` scope are read back to the host. To reduce overhead, enable sink-only readback:

```bash
set NEURON_GPU_SCOPE_READBACK_SINK_ONLY=1
```

This keeps only final (non-consumed) outputs on the host, significantly reducing scope-end overhead for intermediate computations.

---

## 6. Scope Fusion (Experimental)

Scope fusion combines multiple consecutive element-wise operations into a single GPU dispatch, reducing kernel launch overhead.

```bash
set NEURON_GPU_SCOPE_FUSION=1    # default: off
```

Currently supported fusion patterns:
- `binary → binary`
- `binary → fma`
- `binary → binary → fma`

---

## 7. Scope Metrics

Enable detailed GPU scope metrics for performance analysis:

```bash
set NEURON_GPU_SCOPE_METRICS=1
```

This prints per-scope statistics:
- **Dispatch count** — Number of GPU kernel dispatches
- **Barrier count** — Number of synchronization barriers
- **Descriptor writes** — Number of descriptor set writes
- **Readback bytes** — Total bytes transferred from device to host

---

## 8. Parallel For and GPU

The `parallel for` construct is distinct from `gpu { ... }`:

| Construct       | Target                              |
|-----------------|-------------------------------------|
| `parallel for`  | CPU multi-threading (OpenMP-style)  |
| `gpu { ... }`   | GPU kernel dispatch                 |

```npp
// CPU parallelism
parallel for(i is 0; i < n; i++) {
    data[i] is data[i] * 2;
}

// GPU execution
gpu {
    C is A @ B;
}
```

Both can coexist in the same program. The runtime manages resource allocation for each.

---

## 9. Best Practices

1. **Batch operations in a single `gpu` block** — Multiple operations inside one `gpu { ... }` block share a single scope submit, minimizing overhead.

2. **Use fused operations** — Prefer `Y is ReLU(X @ W + b);` over separate matmul, add, and activation steps.

3. **Profile with scope metrics** — Use `NEURON_GPU_SCOPE_METRICS=1` to identify bottlenecks in dispatch count and readback volume.

4. **Minimize readback** — Enable `NEURON_GPU_SCOPE_READBACK_SINK_ONLY=1` when intermediate tensors are not needed on the CPU.

5. **Choose the right backend** — CUDA generally offers better performance on NVIDIA hardware. Vulkan provides broader hardware compatibility.

6. **Tensor size matters** — GPU dispatch provides the most benefit for large tensors (typically 256×256 and above). For small tensors, CPU execution may be faster due to launch overhead.

---

## 10. Environment Variables Reference

| Variable                              | Default | Description                                  |
|---------------------------------------|---------|----------------------------------------------|
| `NEURON_GPU_FORCE_BACKEND`            | `auto`  | Override backend: `auto`, `cpu`, `vulkan`, `cuda` |
| `NEURON_GPU_SCOPE_FUSION`             | `0`     | Enable experimental scope fusion             |
| `NEURON_GPU_SCOPE_METRICS`            | `0`     | Print per-scope performance metrics          |
| `NEURON_GPU_SCOPE_READBACK_SINK_ONLY` | `0`     | Only readback final (non-consumed) outputs   |
