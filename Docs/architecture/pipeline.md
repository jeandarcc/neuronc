# Compiler Pipeline — Pass-by-Pass Reference

## Overview

```
Source (.npp)  →  [Lexer]  →  [Parser]  →  [Frontend]  →  [Sema]
                                                              ↓
                                                        [NIR Builder]
                                                              ↓
                                                        [Optimizer]
                                                              ↓
                                                        [MIR Builder]
                                                              ↓
                                                        [LLVMCodeGen]
                                                              ↓
                                                       Native Binary
```

---

## Stage 1 — Lexer (`src/lexer/`)

**Input:** Raw source bytes (`std::string` / file path)  
**Output:** Flat `std::vector<Token>` stream

| File | Role |
|------|------|
| `Lexer.cpp` | Main tokenizer; handles identifiers, literals, operators, comments, string escapes |
| `Token.cpp` | `Token` struct: kind, value, source range (`line:col`), interned string |

**Key behaviors:**
- Single-pass, no lookahead required — contextual keywords resolved by parser.
- Source locations (`SourceRange`) are attached at this stage and propagated to all subsequent passes for accurate diagnostics.
- Operator sequences (e.g. `>>`) are NOT eagerly coalesced — the parser handles context-sensitive splitting.

---

## Stage 2 — Parser (`src/parser/`)

**Input:** Token stream  
**Output:** Abstract Syntax Tree (AST) nodes — heap-allocated, arena-backed

| File/Directory | Role |
|----------------|------|
| `Parser.cpp` | Top-level entry, delegates to sub-parsers |
| `AST.cpp` | AST node base types and utilities |
| `core/` | Core expression/statement parsing primitives |
| `declarations/` | Function, struct, module, trait, extern declarations |
| `expressions/` | Operators, calls, indexing, lambda, match arms |
| `statements/` | let/var, if/else, loop, return, block, defer |

**Key behaviors:**
- Recursive-descent, hand-written (no parser generator).
- Produces a **typed, source-annotated** AST — node types preserve the semantic category.
- Error recovery: on parse error, attempts to sync to next statement boundary to continue reporting multiple errors.

---

## Stage 3 — Frontend (`src/frontend/`)

**Input:** File path(s), CLI options  
**Output:** Loaded source, pre-processed include graph, initial `DiagnosticContext`

| File | Role |
|------|------|
| `Frontend.cpp` | Source loading, include resolution, encoding validation |
| `Diagnostics.cpp` | Wires up the `DiagnosticContext` for the pipeline |

**Key behaviors:**
- Validates UTF-8 encoding; emits `N1001` if invalid.
- Resolves `import` paths to file-system locations before parsing.

---

## Stage 4 — Semantic Analysis (`src/sema/`)

**Input:** AST  
**Output:** Fully annotated, type-checked AST + symbol table

| File | Role |
|------|------|
| `SemanticAnalyzer.cpp` | Orchestrates all sub-analyzers |
| `SemanticDriver.cpp/.h` | Driver; manages analysis phases and ordering |
| `AnalysisContext.cpp/.h` | Central context: holds symbol table, type arena, diagnostic emitter |
| `AnalysisOptions.h` | Feature flags controlling optional analysis passes |
| `SymbolTable.cpp/.h` | Scoped identifier table; handles shadowing, forward references |
| `ScopeManager.cpp/.h` | Scope push/pop lifecycle |
| `TypeSystem.cpp` | Type representation: primitives, pointers, slices, generics |
| `TypeChecker.cpp/.h` | Type compatibility, coercion rules |
| `TypeResolver.cpp/.h` | Resolves type annotations to concrete types; handles generics |
| `DeclarationAnalyzer.cpp/.h` | Validates struct/fn/trait/impl declarations |
| `ExpressionAnalyzer.cpp/.h` | Type-checks all expression kinds |
| `StatementAnalyzer.cpp/.h` | Validates statement constraints (reachability, return coverage) |
| `FlowAnalyzer.cpp/.h` | Control-flow graph; checks use-before-init, move-after-use |
| `BindingAnalyzer.cpp/.h` | Pattern matching exhaustiveness and binding types |
| `CallableBinder.cpp/.h` | Resolves function/method overloads |
| `ReferenceTracker.cpp/.h` | Tracks reference lifetimes for borrow-checker lite |
| `RuleValidator.cpp/.h` | Additional rules: visibility, mutability, extern constraints |
| `DiagnosticEmitter.cpp/.h` | Semantic-layer diagnostic bridge to `DiagnosticContext` |
| `InputAnalyzer.cpp/.h` | Validates input system semantics (keyboard/gamepad bindings) |
| `GraphicsAnalyzer.cpp/.h` | Validates GPU scope rules, shader constraints, buffer binding |

---

## Stage 5 — NIR Construction + Optimization (`src/nir/`)

**Input:** Annotated AST  
**Output:** Optimized NIR module

| File/Directory | Role |
|----------------|------|
| `NIR.cpp` | NIR data structures (module, function, basic block, instruction) |
| `NIRBuilder.cpp` | Translates AST to NIR |
| `core/` | NIR instruction set definitions |
| `decls/` | NIR-level declaration lowering |
| `types/` | NIR type system |
| `detail/` | Internal helpers |
| `lowering/` | AST → NIR lowering passes |
| `Optimizer.cpp` | Pass manager; runs enabled passes on the NIR module |
| `OptimizerCleanup.cpp` | Dead code elimination, constant folding, trivial inlining |
| `OptimizerGpuScopeLifting.cpp` | Promotes GPU-bound computations to GPU scope |
| `OptimizerTensorFusion.cpp` | Fuses adjacent tensor operations into single kernel calls |
| `OptimizerUtils.cpp` | Shared optimizer utility functions |

---

## Stage 6 — MIR Construction (`src/mir/`)

**Input:** Optimized NIR module  
**Output:** MIR module ready for LLVM code generation

| File | Role |
|------|------|
| `MIRBuilder.cpp` | Main NIR-to-MIR lowering |
| `MIRBuilderBindings.cpp` | Lowers name bindings and variable lifetimes |
| `MIRBuilderLowering.cpp` | Lowers complex expressions (calls, indexing, generics) |
| `MIROwnershipPass.cpp` | Inserts drop calls, validates ownership transfer |
| `MIRPrinter.cpp` | Human-readable MIR dump (for debugging) |

---

## Stage 7 — LLVM Code Generation (`src/codegen/`)

**Input:** MIR module  
**Output:** Object file or JIT-executed native code

| File | Role |
|------|------|
| `LLVMCodeGen.cpp` | MIR → LLVM IR translation; target machine setup; optimization passes |
| `JITEngine.cpp` | OrcJIT setup; used by ncon hot-reload and REPL |
| `llvm/` | LLVM utility wrappers |

**Key behaviors:**
- LLVM IR is generated in SSA form using LLVM's `IRBuilder<>`.
- Target triple is obtained from the CLI flags (`--target`) or defaults to host.
- JIT engine supports incremental compilation for `ncon run --hot-reload`.
