# Windows Installer

Build the Windows installer from the repo root with:

```powershell
.\build_installer.ps1
```

The script performs these steps:

- Builds the `neuron` and `ncon` targets with CMake unless `-SkipBuild` is passed.
- Prepends the detected compiler `bin/` directory to `PATH` so MinGW child tools like `cc1plus.exe` can resolve their runtime DLLs reliably.
- Builds and stages a default `nucleus.exe` runtime under `bin/`.
- Stages `neuron.exe`, `ncon.exe`, `nucleus.exe`, required DLLs, `runtime/src`, `runtime/include`, `runtime/minimal`, `include/`, `src/ncon`, and a bundled GCC toolchain under `build/installer/stage`.
- Invokes Inno Setup and writes the installer into `build/installer/out`.
- Produces release artifacts under `build/installer/release/Neuron-<version>-windows-x64/`.

Common options:

```powershell
.\build_installer.ps1 -SkipInstaller
.\build_installer.ps1 -SkipInstaller -SkipReleaseArtifacts
.\build_installer.ps1 -NoToolchainBundle
.\build_installer.ps1 -ISCCPath "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
.\build_installer.ps1 -Version 0.1.0
```

Installer behavior:

- Reuses the same `AppId`, so a newer installer upgrades the existing installation.
- Adds `{app}\bin` to the Windows `PATH` by default, so both `neuron` and `ncon` are callable everywhere.
- Installs to `C:\Program Files\Neuron` by default.
- Bundles `runtime/` so installed `neuron` can compile from any working directory.
- Bundles minimal SDK sources so installed `neuron build-nucleus --platform ...` works without cloning the repo.
- Registers `.ncon` file association so double-click opens files with `{install}\bin\nucleus.exe`.
- Preserves the previous PATH task selection during upgrade installs.

Release artifacts:

- `Neuron-<version>-windows-x64.exe`: the Windows installer produced by Inno Setup.
- `Neuron-<version>-windows-x64-portable.zip`: a portable archive containing `bin/`, `runtime/`, and bundled toolchain files from the staged tree.
- `SHA256SUMS.txt`: SHA-256 checksums for published artifacts.
- `release-manifest.json`: machine-readable release metadata with artifact names, sizes, and hashes.

Bundled toolchain:

- By default the build script copies `bin`, `include`, `lib`, `libexec`, and MinGW target directories from the compiler prefix discovered via `CMAKE_CXX_COMPILER`.
- GCC plugin development headers under `lib/gcc/*/plugin/include` are pruned from the staged payload to reduce installer size.
- Installed `neuron` prefers `{install}\toolchain\bin` automatically.
- `NEURON_TOOLCHAIN_BIN` can override the toolchain location at runtime.

Minimal runtime builder:

- Installed `neuron` supports `neuron build-nucleus --platform Windows|Linux|Mac`.
- Host platform builds can use bundled toolchain resolution automatically.
- Non-host targets require explicit `--compiler <cross-g++ wrapper path>`.
- Default output is `nucleus.exe` for Windows target and `nucleus` for others.
