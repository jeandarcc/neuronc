# Graphics Subsystem (`runtime/src/graphics/`)

The graphics subsystem provides the rendering backend for the language's native
UI capabilities, 2D/3D visualizations, and windowing integration.

## Architecture & Subdirectories

| Directory/File | Purpose |
|----------------|---------|
| `graphics_core_assets_api.c`, `graphics_assets.c` | Top-level image and 3D model parsing interface. |
| `graphics_window_canvas.c`, `graphics_core_window_canvas_api.c` | Window creation, swapchain handling, and frame orchestration APIs. |
| `graphics_core_state.c` | Internal state tracking for the renderer. |
| `backend/` | The low-level GPU API wrappers (primarily Vulkan, with WebGPU handled in `platform/gpu_webgpu.c`). |
| `platform/` | Integrates with the `SwitchPlatformManager` to create OS-level windows and contexts (HWND, X11 Window, HTML5 Canvas). |
| `scene2d/` | High-level 2D retained-mode scene graph (sprites, text, UI elements). |
| `assets/` | The actual decoders for images (PNG, JPG) and 3D models (OBJ, glTF). |

## Vulkan Backend (`backend/vulkan/`)

Neuron uses Vulkan as its primary native graphics backend. The integration includes:
- **Spir-V Caching:** Shader source (GLSL) is compiled ahead-of-time in `runtime/shaders/` and bundled as SPIR-V binaries inside the `vulkan_shaders.h` header located globally in `runtime/src`.
- **Pipeline Caching:** PSO state is cached heavily to reduce draw-call overhead.
- **Compute Integration:** Interoperates smoothly with `gpu_vulkan/` in the parent directory to allow sharing Vulkan buffers between graphics passes and neural tensor operations.
