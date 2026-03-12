# Architecture Decision Record 0002: Two-Layer IR Design (NIR + MIR)

**Status:** Accepted  
**Date:** 2025  
**Deciders:** NPP core team  
**Categories:** Compiler, IR

---

## Context

After semantic analysis, the compiler needs to transform the AST into a
representation ready for LLVM IR emission. A single-IR approach (AST → LLVM IR
directly) was considered but rejected.

**Problems with direct AST → LLVM IR:**
- Semantic information (high-level types, ownership, tensor shapes) is lost before
  source-language optimizations can operate on it.
- Optimizer passes on high-level constructs (tensor fusion, GPU scope lifting)
  are much easier to express on a typed, high-level IR than on LLVM IR.
- Debugging and diagnostics need a representation closer to the source than LLVM IR.

## Decision

**Adopt a two-layer IR:**

### NIR — Neuron Intermediate Representation (`src/nir/`)
- **Level:** High-level, typed, SSA-based
- **Preserves:** Type system, ownership annotations, tensor shapes, GPU scopes
- **Optimizer passes run here:**
  - `Optimizer.cpp` — pass manager
  - `OptimizerCleanup.cpp` — DCE, constant folding
  - `OptimizerGpuScopeLifting.cpp` — moves compute to GPU scope
  - `OptimizerTensorFusion.cpp` — fuses adjacent tensor ops into single kernels
- **Sub-modules:** `core/`, `decls/`, `types/`, `lowering/`, `detail/`

### MIR — Machine Intermediate Representation (`src/mir/`)
- **Level:** Low-level; models LLVM IR concepts (basic blocks, value bindings)
- **Preserves:** Ownership (via `MIROwnershipPass.cpp`)
- **Role:** Bridge from NIR to `LLVMCodeGen.cpp`; keeps LLVM-specific details out of the optimizer

## Consequences

**Positive:**
- Source-language optimizations (tensor fusion, GPU lifting) operate on NIR where
  the necessary type information is still available.
- `MIRPrinter.cpp` gives a readable dump for debugging without exposing LLVM IR.
- Future IR-level language features (linear types, effect system) can be added to
  NIR without touching the LLVM layer.

**Negative:**
- Two lowering steps add compile-time overhead.
- Bugs can be at either NIR→MIR or MIR→LLVM transitions; requires careful stage tracking.
