# GPU Performance

Tips for maximizing GPU compute throughput in Neuron.

---

## Operation Fusion

The compiler automatically fuses compatible operations:

```npp
gpu {
    result is (a * b) + c;    // fused to single FMA kernel
}
```

---

## Readback Optimization

Set `NEURON_GPU_READBACK_OPT=1` to avoid unnecessary GPUâ†’CPU transfers. Data stays on the GPU until explicitly needed on the CPU.

---

## Metrics

Enable timing metrics:

```bash
NEURON_GPU_DUMP_METRICS=1 neuron run
```

Output includes per-block dispatch time, memory usage, and kernel count.

---

## Best Practices

1. **Minimize data transfers** â€” keep data on the GPU
2. **Batch operations** â€” use single gpu blocks
3. **Use large tensors** â€” GPU overhead not worth it for small data
4. **Profile first** â€” use metrics to find actual bottlenecks

---

## Next Steps

- [Concurrency](../16_concurrency/threads.md) â€” CPU parallelism
