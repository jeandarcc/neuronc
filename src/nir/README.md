# Neuron IR (`src/nir/`)

NIR (Neuron Intermediate Representation) is the high-level, typed, SSA-based
intermediate language of Neuron. It sits between the AST (`src/sema/`) and
the Machine IR (`src/mir/`).

## Architecture & Subsystems

| Directory/File | Purpose |
|----------------|---------|
| `NIR.cpp` | Core NIR data structures: `Module`, `Function`, `Block`, `Instruction`, `Value`, `ConstantInt/Float/String`. Implements human-readable NIR stringification (the `print()` and `renderModule()` routines). |
| `NIRBuilder.cpp` | Main driver for converting a semantically-valid AST into NIR. |
| `core/`, `decls/`, `types/`, `lowering/`, `detail/` | Subsystems responsible for incrementally lowering AST expressions, statements, and types into `InstKind` operations. |
| `Optimizer.cpp` | The core optimization pass manager and high-level transformations. Contains `ConstantFoldingPass`, `CopyPropagationPass`, and `AlgebraicSimplificationPass`. |
| `OptimizerCleanup.cpp` | Dead code elimination (DCE) and block merging. |
| `OptimizerTensorFusion.cpp` | Finds adjacent tensor math ops (e.g. `add` + `relu`) and fuses them into a single `tensor_op` kernel call. |
| `OptimizerGpuScopeLifting.cpp` | Promotes eligible array/tensor operations to GPU scope block boundaries. |

## The Instruction Set (`InstKind`)

Unlike LLVM IR, NIR preserves domain-specific knowledge that would be lost if
lowered too early. Key supported instructions defined in `NIR.cpp`:

- **Arithmetic:** `add`, `fadd`, `pow`, `nthroot`
- **Control Flow:** `br`, `condbr`, `ret`
- **Memory/Type:** `alloca`, `load`, `store`, `cast`
- **High-Level Graphics/Compute:** `gpu_scope_begin`, `gpu_scope_end`
- **Tensor Ops:** `tensor_add`, `tensor_matmul`, `tensor_linear_fused`, `tensor_fma`, `tensor_slice`

## The Optimizer passes

Because NIR retains tensor shapes and GPU scope blocks, domain-specific optimizer
passes act here before the codebase is transformed into flat LLVM pointers:

1. **Algebraic Simplification** (`Optimizer.cpp`): Folds operations like `x * 1 -> x` and `x + 0 -> x`.
2. **Constant Folding** (`Optimizer.cpp`): Evaluates arithmetic between constants at compile time.
3. **Copy Propagation** (`Optimizer.cpp`): Eliminates redundant `load`/`store` chains.
4. **Tensor Fusion** (`OptimizerTensorFusion.cpp`): Scans for independent tensor mathematical instructions and collapses them into fused kernels.
