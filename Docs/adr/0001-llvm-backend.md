# Architecture Decision Record 0001: Use LLVM as the Code Generation Backend

**Status:** Accepted  
**Date:** 2025 (initial implementation)  
**Deciders:** Neuron core team  
**Categories:** Compiler, Codegen

---

## Context

Neuron requires native code generation targeting x86-64 (Windows/Linux/macOS),
WebAssembly, and future RISC-V targets. The compiler needs:

1. A mature, battle-tested optimizer (dead code elimination, inlining, LICM, etc.)
2. Multi-target code generation from a single IR
3. JIT compilation support for REPL and hot-reload
4. First-class C/C++ interoperability
5. Active maintenance and community

Alternative backends considered:
| Backend | Pros | Cons |
|---------|------|------|
| **LLVM** | Industry standard, mature, multi-target, JIT | Large dependency, compile-time overhead |
| QBE | Tiny, fast to compile against | Few targets, no JIT, small community |
| Cranelift | WebAssembly-first, Rust ecosystem | C++ integration friction, less mature optimizer |
| Custom backend | Full control | Enormous implementation cost, reinventing decades of work |

## Decision

**Use LLVM** as the exclusive code generation backend.

- Neuron emits LLVM IR from `src/codegen/LLVMCodeGen.cpp`
- JIT execution uses LLVM's OrcJIT (`src/codegen/JITEngine.cpp`)
- LLVM components are resolved at configure time via `LLVMConfig.cmake`
- The MinGW/MSYS2-distributed LLVM is the canonical build toolchain on Windows

## Consequences

**Positive:**
- Access to LLVM's full optimizer pipeline (O0–O3, LTO)
- Multi-target support (x86, WASM, future RISC-V) via `llvm-config --targets-built`
- OrcJIT enables `ncon`'s hot-reload feature

**Negative:**
- Build-time LLVM dependency (~500MB+ installed size)
- `LLVM_DIR` must be specified at CMake configure time
- LLVM API changes between versions require occasional adaptation in `LLVMCodeGen.cpp`
