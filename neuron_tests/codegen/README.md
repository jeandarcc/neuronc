# Runtime and Code Generation Behavior Specification

## Scope

This folder covers successful compilation cases where the primary oracle is executable behavior. These tests do not assert internal LLVM, MIR, or NIR details; they assert what the compiled `.npp` program does.

Subfolders:

- [arithmetic/README.md](arithmetic/README.md)
- [control_flow/README.md](control_flow/README.md)
- [functions/README.md](functions/README.md)
- [structs/README.md](structs/README.md)
- [optimizations/README.md](optimizations/README.md)

Included:

- Numeric results
- Branching and looping behavior
- Function-call behavior and value passing
- Struct layout as visible through field access and mutation
- Optimization-sensitive but user-visible semantics

Excluded:

- Backend instruction selection details
- Performance-only assertions without semantic impact
- Internal codegen unit coverage

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N5000-N5099` | runtime-visible lowering failures or backend-visible source errors |
| `W5000-W5099` | backend fallbacks or degraded-but-correct behavior |
| `NPP0001` | generic fallback if a backend-visible source failure escapes uncategorized |

## Test List

| Name | `.npp` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `codegen_baseline__integer_arithmetic` | `module System; Init is method() { Print(1 + 2 * 3); }` | Prints `7` | Baseline end-to-end expression execution |
| `codegen_baseline__if_else_branch` | branch on a boolean and print one arm | Prints the correct branch result | Keeps control-flow lowering honest |
| `codegen_baseline__function_call_chain` | nested function calls returning arithmetic values | Prints exact final value | Exercises call/return wiring |
| `codegen_baseline__struct_field_roundtrip` | assign struct fields then print them | Prints expected field values | Covers aggregate storage behavior |
| `codegen_baseline__loop_accumulation` | loop summing a range | Prints exact sum | Ensures loop state survives lowering |
| `codegen_baseline__optimization_preserves_side_effect` | expression with `Print` in one branch | Same visible side effects regardless of optimization level | Prevents incorrect aggressive simplification |

## Edge Cases

- Integer overflow boundaries if language semantics define wrapping or trapping
- Float formatting and precision normalization
- Empty loops, zero-iteration loops, and single-iteration loops
- Recursion depth and tail-like control flow if supported
- Struct field mutation through aliases
- Optimization-sensitive expressions with observable ordering

## Happy Path

Happy-path tests here always compile and execute. The expected output must be complete and deterministic.

## Error Path

Error-path tests in this folder are rare and reserved for user-visible backend failures, unsupported lowering cases, or runtime traps that occur only after semantic success.
