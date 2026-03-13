# Compiler Optimizations

Neuron applies multiple optimization passes to produce efficient code.

---

## Optimization Passes

| Pass | Description | Example |
|------|-------------|---------|
| **Constant Folding** | Evaluate constant expressions at compile time | `2 + 3` → `5` |
| **Dead Code Elimination** | Remove unreachable or unused code | Unused variables removed |
| **Vectorization** | Auto-vectorize loops with SIMD | Element-wise tensor ops |
| **Operation Fusion** | Combine compatible operations | `a * b + c` → FMA |
| **Inlining** | Inline small method calls | Reduces call overhead |

---

## Real Example

From `OptTest.nr`:

```npp
Compute method() {
    x is 2 + 3;       // constant folded to 5
    y is x * 10;       // constant folded to 50
    unused is 99;      // dead code eliminated
    Print(y);
}
```

After optimization, this becomes equivalent to:

```npp
Compute method() {
    Print(50);
}
```

---

## Optimization Levels

Configured in `neuron.toml`:

```toml
[build]
optimize = "aggressive"    # none, basic, aggressive
```

| Level | Passes Applied |
|-------|---------------|
| `none` | No optimization |
| `basic` | Constant folding, dead code elimination |
| `aggressive` | All passes including vectorization, fusion, inlining |

---

## Tensor-Specific Optimizations

- **FMA fusion** — `(a * b) + c` becomes a single fused multiply-add
- **Kernel caching** — Compiled GPU kernels cached to disk
- **Auto-tuning** — Runtime selection of optimal kernel parameters

---

## Viewing Optimized Output

```bash
neuron compile file.nr    # Show LLVM IR (optimized)
```
