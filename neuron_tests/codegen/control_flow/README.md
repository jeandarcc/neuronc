# Runtime Control Flow Specification

## Scope

This folder covers observable behavior of branching, loops, early exits, and exception-style control flow after successful compilation.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N5000-N5099` | runtime-visible control-flow lowering failure |
| `N2000-N2099` | semantic rejection of invalid control-flow inputs |
| `NR0001`, `NR2001` | current fallbacks |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `flow__if_else_true_branch` | `module System; Init is method() { if(true) { Print("T"); } else { Print("F"); } }` | Prints `T` | Smallest branch correctness test |
| `flow__if_else_false_branch` | same as above with `false` | Prints `F` | Ensures both branch directions execute correctly |
| `flow__while_loop_counter` | `module System; Init is method() { i is 0; while(i < 3) { Print(i); i++; } }` | Prints `0`, `1`, `2` in order | Baseline loop state update |
| `flow__for_loop_sum` | `module System; Init is method() { sum is 0; for(i is 0; i < 4; i++) { sum is sum + i; } Print(sum); }` | Prints `6` | Canonical counted loop |
| `flow__break_and_continue` | loop that skips one value and breaks on another | Prints only the documented subset in order | Verifies edge exits inside loops |
| `flow__try_catch_finally_order` | `module System; Init is method() { try { throw "boom"; } catch(error) { Print(error); } finally { Print("done"); } }` | Prints `boom` then `done` | Ensures exceptional control-flow order is preserved |

## Edge Cases

- Zero-iteration loops
- Nested loops with inner and outer breaks
- `return` inside conditionals and loops
- `finally` blocks after normal completion vs after throw
- `switch`/`match`-style constructs with default arms
- Deep nesting and many labels/branches in one method

## Happy Path

Happy-path tests should specify exact output order because control-flow bugs often preserve values but scramble sequencing.

## Error Path

Error-path tests in this folder are reserved for backend-visible miscompiles, runtime traps, or unsupported constructs that pass semantic analysis.
