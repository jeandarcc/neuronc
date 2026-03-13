# GPU Backends

Neuron selects GPU computing backends in priority order with CPU fallback.

---

## Backend Priority

| Priority | Backend | Requirements |
|----------|---------|-------------|
| 1 | **CUDA** | NVIDIA GPU + CUDA toolkit |
| 2 | **Vulkan** | Any GPU with Vulkan driver |
| 3 | **CPU** | Always available (fallback) |

---

## Environment Variables

| Variable | Description |
|----------|-------------|
| `NEURON_GPU_FORCE_BACKEND` | Force a specific backend (`cuda`, `vulkan`, `cpu`) |
| `NEURON_GPU_DUMP_METRICS` | Print GPU performance timing after each block |
| `NEURON_GPU_READBACK_OPT` | Skip unnecessary GPU-to-CPU data transfers |

---

## Checking GPU Availability

The runtime automatically detects available GPUs. Use `NEURON_GPU_DUMP_METRICS=1` to see which backend was selected.

---

## Next Steps

- [Scope Batching](scope_batching.md) — Batched GPU dispatch
