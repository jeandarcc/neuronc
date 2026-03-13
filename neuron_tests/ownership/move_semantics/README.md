# Ownership Move Semantics Specification

## Scope

This folder covers ownership transfer with `move`, post-move invalidation, reinitialization after move, and transfer across calls and returns.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N3000-N3099` | use-after-move, double move, move from invalid source |
| `W3000-W3099` | needless move or other quality-only move diagnostics |
| `NR2001` | current semantic fallback |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `move__basic_transfer` | `Init is method() { x is 100; y is move x; Print(y); }` | Compiles and prints `100` | Baseline transfer contract |
| `move__use_after_move` | `Init is method() { x is 100; y is move x; Print(x); }` | `N3000-N3099` | Core safety guarantee |
| `move__double_move_rejected` | `Init is method() { x is 100; y is move x; z is move x; }` | `N3000-N3099` | Prevents repeated transfer from dead owner |
| `move__return_transferred_value` | `Take method(value as int) as int { return value; } Init is method() { x is 7; Print(Take(move x)); }` | Compiles and prints `7`; later use of `x` is invalid if attempted | Covers call-boundary ownership transfer |
| `move__reinitialize_after_move` | `Init is method() { x is 1; y is move x; x is 2; Print(x); Print(y); }` | Compiles and prints `2` then `1` | Defines whether rebinding revives a moved name |
| `move__needless_move_warning` | move from an immediately consumed temporary or trivially copied value | Warning in `W3000-W3099` or documented no-warning success | Preserves quality diagnostics without overconstraining semantics |

## Edge Cases

- Move inside one branch then use after join point
- Move from fields, array elements, or dereferenced pointers if supported
- Move into overloaded and generic functions
- Move from parameters vs locals
- Move after aliasing through `is`

## Happy Path

Happy-path tests should prove the new owner is the only valid owner after transfer unless the language explicitly defines shared behavior for the value category involved.

## Error Path

Error-path tests should capture both the move origin and the invalid later use, especially when the compiler can point to both sites.
