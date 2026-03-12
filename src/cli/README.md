# CLI, Build, & Web Pipeline (`src/cli/`)

Despite its name, `src/cli/` is the orchestrator for the entire compiler. It handles
command parsing, project generation, package dependency fetching, macro expansion
in config files, and the `web` WASM build pipeline.

## Core Dispatch & Projects

| File/Subsystem | Purpose |
|----------------|---------|
| `CommandDispatcher.cpp` | The top-level router for `neuron build`, `neuron run`, etc. Routes to specific executors in `commands/`. |
| `ProjectConfig.cpp`, `ProjectGenerator.cpp` | Parses the workspace layout, generates CMake/Ninja structures for C/C++ native modules, and invokes `neuronc` on the `.npp` source files. |
| `ProductBuilder.cpp`, `ProductSettings.cpp` | Manages the final output structure (`.exe`, `.so`, or the `pkg/` bundles). |

## Build & Module Management

| File/Subsystem | Purpose |
|----------------|---------|
| `PackageManager.cpp`, `PackageLock.cpp` | Handles fetching, verifying, and caching GitHub/HTTP dependencies into `%LOCALAPPDATA%`, using `neuron.lock`. |
| `ModuleCppSupport.cpp`, `ModuleCppManifest.cpp` | Bridges external C/C++ libraries declared in `[modulecpp]` tables into the NPP build graph. |

## Web Assembly Target (`neuron build --target=web`)

NPP treats the web as a first-class target. The Emscripten integration is housed here:
- `WebBuildPipeline.cpp`: Converts the project into `.wasm`, hooks up Emscripten (`em++`), and packages the `index.html`.
- `WebDevServer.cpp`: An embedded HTTP server launched by `neuron run --target=web`. Injects SharedArrayBuffer isolation headers.
- `WebShaderTranspiler.cpp`: Translates compiled Vulkan SPIR-V into WGSL so NPP graphics code can run on WebGPU seamlessly.

## Settings Macro Engine

`src/cli/` includes a powerful custom macro engine (`SettingsMacroProcessor.cpp`,
`SettingsMacroParsing.cpp`, etc.) that expands variables within `.modulesettings`
files. This is used to dynamically construct include paths spanning the user's
local machine and the global package cache.

## Subdirectories

- `commands/`: Individual command implementations (Build, Run, Publish).
- `repl/`: The interactive prompt loop.
- `installer/`: Handles self-packaging and registry setup on Windows.
- `pipeline/`: Stages of the standard native AOT build.
- `templates/`: Boilerplate injections, including the HTML/JS glue code used by `WebBuildPipeline`.
