# Watch & Hot Reload

The `ncon watch` command monitors source files and automatically rebuilds and hot-reloads the running program.

---

## Usage

```bash
neuron ncon watch
```

---

## How It Works

1. **Watches** all `.nr` files in `src/`
2. On file change â†’ **rebuilds** the NCON container
3. If the change is **compatible** â†’ **hot patches** the running VM
4. If the change is **structural** â†’ **full restart**

---

## Compatible Changes (Hot Patchable)

- Method body edits
- Constant value changes
- Print statement changes

---

## Structural Changes (Full Restart)

- Adding/removing classes or methods
- Changing method signatures
- Adding/removing fields

---

## Enabling in Config

```toml
[ncon]
hot_reload = true
```

---

## Next Steps

- [Sandbox](sandbox.md) â€” Security isolation
- [Inspect](inspect.md) â€” Container analysis
