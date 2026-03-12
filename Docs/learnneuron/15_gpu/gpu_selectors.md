# GPU Selectors

Control which GPU device is used with the `gpu(...)` selector syntax.

---

## Prefer vs Force

```npp
// Prefer discrete GPU (falls back to any available)
gpu (prefer: discrete) {
    result is A @ B;
}

// Force discrete GPU (error if unavailable)
gpu (force: discrete) {
    result is A @ B;
}
```

---

## Full Selector Form

```npp
gpu (policy: GPUPolicy.Prefer, mode: GPUMode.Discrete) {
    result is A + B;
}
```

---

## Selector Options

| Key | Values | Description |
|-----|--------|-------------|
| `prefer` | `discrete`, `integrated` | Prefer a device type, fallback to others |
| `force` | `discrete`, `integrated` | Require a device type, error if missing |
| `policy` | `GPUPolicy.Prefer`, `GPUPolicy.Force` | Explicit policy enum |
| `mode` / `target` | `GPUMode.Discrete`, `GPUMode.Integrated` | Explicit device enum |

---

## Without Selector

`gpu { }` without a selector uses the system default (typically the most powerful available GPU).

---

## Next Steps

- [Backends](backends.md) — GPU backend priority
