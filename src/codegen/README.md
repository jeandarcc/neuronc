# Code Generation (`src/codegen/`)

This directory is the final stop in the compiler pipeline. It translates MIR
(Machine IR) into LLVM IR, and then either compiles it to a native object file
or executes it immediately via JIT.

## Core Files & Subdirectories

| File / Directory | Purpose |
|------------------|---------|
| `LLVMCodeGen.cpp` | The immense `~150k` byte translation module. Translates `MIRModule` instructions directly into LLVM IR via `llvm::IRBuilder`. Handles creating functions, basic blocks, `allocas`, and math primitives. |
| `JITEngine.cpp` | Powers the internal execution engine using `llvm::orc::LLJIT`. Compiles LLVM IR modules directly to executable memory for REPL and hot-reloading scenarios without producing a `.o`/`.exe` file. |
| `llvm/` (Dir) | Abstractions and wrappers around the LLVM C++ API. Keeps the heavy dependencies localized so the rest of the compiler isn't burdened by slow compilation times. |

## Architecture Notes

### The LLVM Dependency Isolation Rule
This is the **only** directory in `src/` allowed to link against LLVM. 
By keeping `#include <llvm/...>` out of the AST, `sema`, and NIR layers,
the overall compiler remains portable and compilation times stay low.

### Pass Management
Neuron relies on LLVM for all standard target-independent optimizations (O1-O3):
- Instruction combining (InstCombine)
- Dead code elimination (DCE)
- Function inlining
- Loop invariant code motion (LICM)

These are configured within `LLVMCodeGen.cpp` immediately after the translation
from MIR.

### Resolving Runtime Symbols for JIT
`JITEngine.cpp` dynamically resolves symbols against the host process. This
allows JITed bytecode to call C functions exported by the `runtime/` native
library (like `Tensor_Add` or `Process_Run`).
