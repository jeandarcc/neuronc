# Semantic Analysis (`src/sema/`)

The Semantic Analyzer (`sema`) is the most complex phase of the Neuron compiler.
It takes a raw, syntactic AST from `src/parser/` and produces a fully type-checked,
name-resolved, and semantically valid AST ready for NIR lowering.

## Subsystems

This directory contains ~40 files, split into focused analyzer classes that all
share a central `AnalysisContext`.

### Orchestration & Context
| File | Role |
|------|------|
| `SemanticAnalyzer.cpp` | Top-level entry point called from `main`. |
| `SemanticDriver.cpp/.h` | The actual driver loop that sequences the sub-analyzers. |
| `AnalysisContext.cpp/.h` | Central state: holds the `SymbolTable`, `TypeSystem`, diagnostic emitter, and shared caches. |
| `AnalysisHelpers.cpp/.h` | Common utilities for AST traversal and semantic checks. |
| `AnalysisOptions.h` | Feature flags and strictness settings for the driver. |

### Type System & Environments
| File | Role |
|------|------|
| `TypeSystem.cpp` | Representation of all Neuron types (`Primitive`, `Struct`, `Pointer`, `Tensor`, etc.) in an arena. |
| `TypeChecker.cpp/.h` | Logic for type compatibility, subtyping, and coercions (e.g., implicit widening). |
| `TypeResolver.cpp/.h` | Resolves syntax-level type annotations (`AST::Type`) into semantic `TypeSystem` objects. |
| `SymbolTable.cpp/.h` | Hierarchical name environment handling variable shadowing and forward decls. |
| `ScopeManager.cpp/.h` | Scope lifecycle management; handles push/pop during traversal. |

### Statement & Expression Analysis
| File | Role |
|------|------|
| `DeclarationAnalyzer.cpp/.h` | First pass: collects `fn`, `struct`, and `trait` signatures before parsing bodies. |
| `ExpressionAnalyzer.cpp/.h` | Heaviest pass: computes types for math, logic, calls, member access. |
| `StatementAnalyzer.cpp/.h` | Validates scoping, reachability (dead code post-`return`), loop break validity. |

### Advanced Flow & Ownership
| File | Role |
|------|------|
| `FlowAnalyzer.cpp/.h` | Builds a control-flow graph (CFG) to enforce definitely-initialized variables and move semantics. |
| `ReferenceTracker.cpp/.h` | Validates borrow lifetimes and ensures mutable borrows are exclusive (borrow-checker lite). |
| `BindingAnalyzer.cpp/.h` | Validates `match` exhaustiveness and let-binding pattern structures. |
| `CallableBinder.cpp/.h` | Method overload resolution and generic instantiation. |

### Domain-Specific Analyzers
| File | Role |
|------|------|
| `GraphicsAnalyzer.cpp/.h` | Validates GPU scopes (`@gpu`), checks that `tensor` buffers obey Vulkan backend rules. |
| `InputAnalyzer.cpp/.h` | Validates game/engine input bindings for the event loop design. |
| `RuleValidator.cpp/.h` | Leftover static checks: visibility (`pub`), extern C ABI constraints, mutability constraints. |

### Error Reporting
| File | Role |
|------|------|
| `DiagnosticEmitter.cpp/.h` | Wraps the global context to emit `N2xxx` (semantic) and `N3xxx` (ownership) errors using AST source locations. |

## Architecture Notes

### Multi-Pass Design
Because Neuron allows forward references (calling a function defined later in
the file), `sema` is strictly multi-pass:
1. **Declaration Pass (`DeclarationAnalyzer`)**: Scans root-level structs and function signatures.
2. **Body Pass (`ExpressionAnalyzer`/`StatementAnalyzer`)**: Resolves inside function bodies.

### Memory & Pointers
- The AST is deeply mutable during this phase. `ASTNode::setType()` is called constantly.
- `TypeSystem` pointers are permanently owned by the `AnalysisContext` arena. A
  `Type*` is valid for the entirety of the compilation session.

### Adding a Pass
If you are adding a new semantic rule, consider adding it to `RuleValidator.cpp`
unless it warrants an entire AST traversal on its own. If it does, create a new
`FooAnalyzer` class and hook it into `SemanticDriver::run()`.
