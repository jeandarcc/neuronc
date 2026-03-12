# Watch & Hot Reload

The `ncon watch` command monitors source files and automatically rebuilds and hot-reloads the running program.

---

## Usage

```bash
neuron ncon watch
```

---

## How It Works

1. **Watches** all `.npp` files in `src/`
2. On file change → **rebuilds** the NCON container
3. If the change is **compatible** → **hot patches** the running VM
4. If the change is **structural** → **full restart**

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

- [Sandbox](sandbox.md) — Security isolation
- [Inspect](inspect.md) — Container analysis
