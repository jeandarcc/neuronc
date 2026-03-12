# Platform Abstraction (`runtime/src/platform/`)

The `platform/` directory is the **copper wall** of the runtime. No other part of the
runtime (`io.c`, `tensor.c`, etc.) is permitted to include OS-specific headers
like `<windows.h>` or `<unistd.h>`. All OS interactions must pass through the interfaces
defined in `platform_manager_internal.h`.

## Subsystems

| Directory/File | Target OS / Purpose |
|----------------|---------------------|
| `win32/` | Windows 10/11 (Win32 API, UI threads, XInput) |
| `posix/` | Linux / BSD (pthreads, epoll, X11/Wayland) |
| `apple/` | macOS / iOS (Mach threads, Metal hooks) |
| `web/` | WebAssembly (Emscripten JS interop, WebWorkers) |
| `common/` | Shared platform-agnostic fallback implementations. |
| `gpu_webgpu.c` | WebGPU bindings specifically for the Emscripten/WASM web target. |
| `platform_manager.c` | Core router and bootstrap logic. |

## SwitchPlatformManager Pattern

The runtime uses pseudo-object-oriented C to implement a virtual function table
for platform capabilities.

1. `platform_manager.c` defines a singleton struct containing function pointers.
2. At startup, the runtime calls `Platform_Initialize()`.
3. The specific backend (e.g., `win32/platform_win32_init.c`) populates the function
   pointers with its OS-specific implementations.

**Capabilities abstracted:**
- Thread creation and synchronization (mutex, condvar)
- High-resolution timing
- Dynamic library loading (`LoadLibrary` / `dlopen`)
- File I/O and memory mapping
- Window creation and event polling

## Adding a New Platform Port

To port Neuron++ to a new OS (e.g., a real-time OS or a new console):
1. Create `runtime/src/platform/<new_os>/`.
2. Implement all functions required by `platform_manager_internal.h`.
3. Update `CMakeLists.txt` to compile your folder when the target matches.
4. *Do not touch any code outside of your new folder.*
