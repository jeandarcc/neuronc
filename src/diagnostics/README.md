# Diagnostics Engine (`src/diagnostics/`)

This directory houses the multi-locale error reporting system for the Neuron
compiler. Rather than hardcoding English error messages into the compiler passes,
errors are emitted by ID (e.g., `N2042`) and resolved to formatted messages at
runtime.

## Architecture

| File | Purpose |
|------|---------|
| `DiagnosticLocale.cpp` | Represents a single loaded locale (e.g., `de`, `zh`, `en-US`), holding the parsed dictionary of ID → Message templates derived from the TOML files in `config/diagnostics/`. |
| `DiagnosticLocalizer.cpp` | The core engine. Discovers locale definition files on disk, handles fallback (always falls back to `en` if a string is missing in the target locale), and formats templates by substituting `{keys}`. |

## The Data Flow

1. An analyzer in `src/sema/` detects a mismatch.
2. It calls `ctx.emitter.emit(N2042, location, {{"got", "i32"}, {"expected", "f32"}})`.
3. The emitter forwards the ID, source location, and arguments to `DiagnosticLocalizer`.
4. `DiagnosticLocalizer` looks up `N2042` in the active `DiagnosticLocale`.
5. The template `"cannot assign value of type '{got}' to variable of type '{expected}'"` is retrieved.
6. The string is formatted with the provided arguments and printed to `stderr` (or sent over LSP).

## Locales on Disk

The actual locale dictionaries are located in `config/diagnostics/`. A python tool
(`plugger.py`) is used by contributors to add strings across all locales simultaneously
to avoid missing IDs.
