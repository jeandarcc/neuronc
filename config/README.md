# Configuration System (`config/`)

This directory holds environment-independent configuration data used by the
Neuron toolchain.

## Directory Layout

| Directory / File | Purpose |
|------------------|---------|
| `builtin.modulesettings` | Default configuration for the `builtin` module namespace. Defines compiler flags injected into all projects. Evaluated by `src/cli/SettingsMacroProcessor.cpp`. |
| `cli/` | Configuration related specifically to the `neuron` command line interface (e.g., `help.toml` for overriding usage text). |
| `diagnostics/` | The multi-locale error code catalog. |

## Why configuration lives here instead of source code

Neuron separates strings, default paths, and error catalogs from C++ source code.
This has three major advantages:
1. **Localization:** Translators can edit TOML files without needing a C++ compiler (`config/diagnostics/`).
2. **Re-skinning:** Tooling vendors embedding the compiler can swap `help.toml` to brand the CLI dynamically.
3. **Compile times:** Changing a diagnostic message doesn't trigger a 5-minute C++ rebuild.

At build time, `scripts/build.bat` copies the necessary `config/` folders into the output
binary directory so the executable can find them locally, keeping the executable portable.
