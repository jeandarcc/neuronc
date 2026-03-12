# NCON Sandbox

NCON containers run inside a security sandbox that restricts access to the host system.

---

## What Gets Sandboxed?

| Resource | Access |
|----------|--------|
| File system | Restricted to project directory |
| Network | Blocked by default |
| Process spawning | Blocked |
| System calls | Filtered through security layer |
| Memory | Bounded (configurable limit) |

---

## How It Works

On Windows, the NCON VM uses Job Objects to enforce resource limits. On Linux/macOS, seccomp/sandbox profiles are used.

---

## Token Restrictions

Certain language features are restricted in sandboxed mode:

- `unsafe` blocks are **rejected**
- `extern` calls are **rejected**
- `modulecpp` requires explicit whitelist

---

## Configuration

Sandbox settings can be configured in `neuron.toml`:

```toml
[ncon.sandbox]
allow_network = false
allow_filesystem = "project_only"
max_memory_mb = 512
```

---

## Next Steps

- [Inspect](inspect.md) — Examining container contents
