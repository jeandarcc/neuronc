# Architecture Decision Record 0003: ncon Package Manifest Format (neuron.toml)

**Status:** Accepted  
**Date:** 2025  
**Deciders:** Neuron core team  
**Categories:** Package Manager, ncon

---

## Context

Neuron needs a package management system (`ncon`) for dependency declaration,
resolution, sandboxed execution, and bytecode caching. A manifest format must be
chosen that is:

1. Human-writable and readable
2. Parseable without pulling in a JSON/YAML parser dependency
3. Extensible for native config, build flags, and platform overrides
4. Deterministic (lockfile support)

**Alternatives considered:**
| Format | Pros | Cons |
|--------|------|------|
| **TOML** | Human-friendly, well-specified, typed | Requires a TOML parser |
| JSON | Universally supported | Verbose, no comments, poor human-writability |
| YAML | Flexible | Notoriously complex spec, parsing footguns |
| Custom DSL | Full control | Maintenance burden, no ecosystem tooling |

## Decision

**Use TOML** as the manifest format, stored in `neuron.toml`.

### Canonical dependency syntax (inline table):
```toml
[dependencies]
foo = { github = "owner/repo", version = "^1.2.0" }
```

### Native module configuration:
```toml
[modulecpp]
include_dirs = ["vendor/include"]

[modulecpp.MyLib]
sources = ["src/mylib.cpp"]
```

### Lockfile: `neuron.lock`
A companion lockfile pins all resolved dependency versions and content hashes
to guarantee deterministic builds across machines.

### Key files in `src/ncon/`:
- `Manifest.cpp` — TOML parsing and validation
- `Builder.cpp` — dependency graph construction and resolution
- `Sandbox.cpp` — isolated execution environment
- `VMExecutorLifecycle.cpp` + siblings — bytecode execution engine

## Consequences

**Positive:**
- TOML's inline table syntax cleanly represents dependency tuples.
- The `[modulecpp]` table maps naturally to CMake target properties.
- Lockfile guarantees reproducible CI builds.

**Negative:**
- A TOML parser (currently custom/embedded) must be maintained.
- Legacy `[ncon.modulecpp.<Name>]` syntax still parses as an alias —
  this backward compatibility creates two valid syntaxes for the same thing.
