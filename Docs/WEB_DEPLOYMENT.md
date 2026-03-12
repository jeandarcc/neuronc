# Web Deployment

This document describes the first-class web target flow:

- `neuron build --target=web`
- `neuron run --target=web`

## Prerequisites

- Emscripten toolchain must be installed and active in the shell.
- `em++` must be resolvable from `PATH` (or by the toolchain resolver).
- Build host must be able to run the normal `neuron` CLI.

## Configuration (`neuron.toml`)

Web target behavior is controlled by the `[web]` section:

```toml
[web]
canvas_id = "neuron-canvas"
wgsl_cache = true
dev_server_port = 8080
enable_shared_array = true
initial_memory_mb = 64
maximum_memory_mb = 512
wasm_simd = true
```

Key notes:

- `canvas_id`: canvas element id used by loader glue.
- `wgsl_cache`: enables build-time SPIR-V to WGSL cache reuse.
- `dev_server_port`: port used by `neuron run --target=web`.
- `enable_shared_array`: enables pthread/shared-array linker flags.
- `initial_memory_mb` / `maximum_memory_mb`: wasm memory bounds.
- `wasm_simd`: enables wasm SIMD (`+simd128`, `-msimd128`).

## Build Flow

`neuron build --target=web` performs:

1. Compiles the project entry into a wasm object (`wasm32-unknown-emscripten`).
2. Transpiles discovered `.spv` shaders to WGSL (AOT) with cache support.
3. Links runtime + program with `em++` into web artifacts.
4. Generates web entry files from templates.

Default output directory:

- `build/web/index.html`
- `build/web/loader.js`
- `build/web/app.js`
- `build/web/app.wasm`
- `build/web/shaders/*.wgsl` (when shader inputs exist)

## Run Flow

`neuron run --target=web`:

1. Triggers the web build pipeline.
2. Starts embedded zero-dependency dev server.
3. Serves `build/web` and opens browser (unless `--no-open` is passed).

The dev server always returns required isolation headers:

- `Cross-Origin-Opener-Policy: same-origin`
- `Cross-Origin-Embedder-Policy: require-corp`
- `Cross-Origin-Resource-Policy: same-origin`

This is required for SharedArrayBuffer-enabled thin-client scenarios.

