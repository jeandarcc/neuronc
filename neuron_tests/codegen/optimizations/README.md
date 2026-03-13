# Runtime Optimization Semantics Specification

## Scope

This folder covers programs whose meaning must remain unchanged even when the compiler performs optimizations. The oracle is visible output, side effects, and runtime success, not optimizer internals.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N5000-N5099` | wrong-code, dropped side effect, or runtime failure caused by optimized lowering |
| `W5000-W5099` | backend falls back to a less optimized but still correct path |
| `NR0001` | current generic fallback |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `opt__constant_fold_preserves_result` | `module System; Init is method() { Print((2 * 3) + 4); }` | Prints `10` | Smallest optimization-sensitive arithmetic case |
| `opt__dead_branch_does_not_run_side_effect` | `module System; Init is method() { if(false) { Print("dead"); } Print("live"); }` | Prints only `live` | Guards branch simplification correctness |
| `opt__common_subexpression_visible_result` | repeated pure arithmetic stored then printed | Prints the mathematically correct result | Protects reused-expression correctness |
| `opt__fusion_chain_keeps_output` | deterministic tensor or fluent-chain example with known output | Same output as unoptimized reference | Captures user-visible effects of high-level fusion |
| `opt__side_effect_order_preserved` | `module System; Trace method(text as string) as int { Print(text); return 1; } Init is method() { Print(Trace("A") + Trace("B")); }` | Prints `A`, `B`, then `2` in the documented order | Prevents illegal reordering |
| `opt__unreachable_warning_no_wrong_code` | method with early `return` and later `Print` | Runtime omits dead code while warning behavior stays stable | Bridges semantic warnings with runtime correctness |

## Edge Cases

- Side effects hidden inside arguments or helper methods
- Tensor/gpu optimizations that may reorder operations
- Aliasing and ownership-sensitive optimizations
- Constant folding near integer or float boundaries
- Backend fallback when an optimization is intentionally disabled

## Happy Path

Happy-path tests should compare optimized programs to a source-level reference meaning, not to IR text.

## Error Path

Error-path tests should document the observed wrong behavior precisely: missing output, extra output, wrong order, wrong numeric result, or runtime crash.
