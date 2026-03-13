# Ownership Behavior Specification

## Scope

This folder covers `.nr` ownership, aliasing, copying, borrowing, and lifetime rules that are visible through compilation or program execution.

Subfolders:

- [move_semantics/README.md](move_semantics/README.md)
- [borrowing/README.md](borrowing/README.md)
- [lifetimes/README.md](lifetimes/README.md)

Included:

- `is` aliasing semantics
- `another` copy semantics where they interact with ownership guarantees
- `move` invalidation and transfer
- references or aliases that outlive valid storage
- ownership-sensitive diagnostics and warnings

Excluded:

- Parser-only keyword recognition
- Internal MIR ownership-pass details

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N3000-N3099` | invalid move behavior |
| `N3100-N3199` | invalid borrow or alias behavior |
| `N3200-N3299` | invalid lifetime extension or escape |
| `W3000-W3099` | suspicious but legal ownership patterns |
| `NR2001` | current semantic fallback when ownership-specific codes are absent |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `ownership_baseline__alias_by_default` | `Init is method() { x is 10; y is x; y is 20; Print(x); }` | Compiles and prints `20` | Documents the language’s alias-by-default contract |
| `ownership_baseline__deep_copy_via_another` | `Init is method() { x is 10; y is another x; y is 20; Print(x); }` | Compiles and prints `10` | Distinguishes copy from alias |
| `ownership_baseline__use_after_move` | `Init is method() { x is 10; y is move x; Print(x); }` | `N3000-N3099` | Core ownership safety rule |
| `ownership_baseline__return_valid_owner` | return moved or copied value from helper | Compiles and returns/prints expected value | Validates transfer across call boundaries |
| `ownership_baseline__dangling_alias` | alias returned from inner scope | `N3200-N3299` | Ensures lifetime analysis protects scope exits |
| `ownership_baseline__warning_needless_move` | move from temporary or immediately discarded value | `W3000-W3099` or success without behavior change | Captures quality diagnostics without blocking compilation |

## Edge Cases

- Moves inside conditional branches and loops
- Aliases to collection elements, fields, or indexed values
- Rebinding a moved variable before reuse
- Nested scopes where a borrow outlives an inner owner
- Passing values into overloaded or generic functions
- Interactions between `is`, `another`, and `move` in one expression chain

## Happy Path

Ownership happy-path tests should demonstrate legal alias, copy, and move behavior with observable outputs that prove the semantic distinction.

## Error Path

Ownership error-path tests should capture both the invalid use site and the origin of the ownership state change whenever that provenance is visible in diagnostics.
