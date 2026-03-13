# The `ncon` Package Manager (`src/ncon/`)

`ncon` (Neuron Container) is the official package manager, build orchestrator,
and runtime execution engine (sandbox VM) for the Neuron ecosystem.

Unlike most package managers which only download source, `ncon` actively controls
the `neuronc` build pipeline and contains a full bytecode execution VM.

## Subsystems & Files

| Subsystem                  | Files                                             | Purpose                                                                                                                               |
| -------------------------- | ------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------- |
| **CLI Dispatch**     | `NconCLI.cpp`, `NconMiniCLI.cpp`              | Command-line parsing for package operations (`ncon build`, `ncon run`, `ncon add`).                                             |
| **Manifest & Graph** | `Manifest.cpp`, `Builder.cpp`, `Runner.cpp` | Parsing `neuron.toml`, resolving the topological dependency graph, and orchestrating compiler invocations.                          |
| **Native Add-ons**   | `NativeModuleManager.cpp`                       | Compiling and linking C/C++ native addons specified in `[modulecpp]` or `[builtin]`. Ties into the CMake environment dynamically. |
| **Lockfiles**        | `Sha256.cpp`                                    | Content-hashing for determining cache freshness and `neuron.lock` validation.                                                       |

### The ncon Sandbox VM

The VM executes `neuronc`-emitted `.ncon` bytecode. This is mostly used for build
scripts, macro execution, and hot-reload development servers.

| Subsystem                 | Files                                                          | Details                                                                                                                    |
| ------------------------- | -------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| **Format & IO**     | `Bytecode.cpp`, `Format.cpp`, `Reader.cpp`               | Structure, serialization, and deserialization of the `.ncon` binary payload.                                             |
| **Execution Loop**  | `VM.cpp`, `VMExecutorLifecycle.cpp`, `VMExecutorOps.cpp` | Stack-based dispatch loop, setup, and teardown.                                                                            |
| **Values & Memory** | `VMExecutorValues.cpp`, `VMExecutorTensor.cpp`             | Boxed value representations and specific tensor math dispatches over FFI.                                                  |
| **Security**        | `Sandbox.cpp`, `Inspect.cpp`                               | Isolates system calls, restricts I/O, prevents network access unless explicitly granted in `neuron.toml`.                |
| **Hot Reload**      | `VMHotReload.cpp`, `VMExecutorPatch.cpp`                   | In-memory code patching (`ncon run --hot-reload`) replacing functional implementations without losing the VM heap state. |
| **Host FFI**        | `RuntimeBridge.cpp`                                          | Seamless bridge connecting the Sandboxed VM to the compiled `runtime/` native library.                                   |
