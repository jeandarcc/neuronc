# Frontend (`src/frontend/`)

The Frontend bridges the gap between the user's invocation (CLI/LSP) and the
actual compiler pipeline. It handles source file loading, include graphs, and
diagnostic initialization.

## Files

| File | Purpose |
|------|---------|
| `Frontend.cpp` | Main driver for loading source text. Validates UTF-8 encoding. Resolves module imports against the file system. |
| `Diagnostics.cpp` | Wires up the `DiagnosticContext` that will be passed through to `sema` and `nir`. |

## Architecture Notes

- **Import Resolution:** Translates `import foo.bar` statements into physical
  file paths, checking the local project directory and the `ncon` global package cache.
- **Encoding:** Neuron++ strictly requires UTF-8 source files. `Frontend.cpp`
  checks for byte-order marks (BOM) or invalid UTF-8 sequences and emits the `N1001`
  diagnostic early to prevent parser assertions.
