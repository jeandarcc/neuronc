# Ownership Diagnostic Specification

## Scope

This folder covers message quality for move, borrow, alias, and lifetime diagnostics.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N3000-N3099` | move diagnostics |
| `N3100-N3199` | borrow/alias diagnostics |
| `N3200-N3299` | lifetime and escape diagnostics |
| `W3000-W3099` | ownership warnings |
| `NPP2001` | current fallback |

## Test List

| Name | `.npp` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `diag_owner__use_after_move_mentions_origin` | `x is 1; y is move x; Print(x);` | Message states value was moved and, if possible, points to move origin | Ownership diagnostics need provenance |
| `diag_owner__double_move` | two `move x` uses | Message makes clear the second move is invalid because ownership already transferred | Avoids vague duplicate-use wording |
| `diag_owner__dangling_alias_escape` | return alias or pointer to local | Message states local value does not live long enough | Lifetime wording should communicate scope failure directly |
| `diag_owner__alias_after_move` | alias created before move, then dereferenced after move | Message distinguishes alias misuse from plain use-after-move | Important for debugging alias-heavy code |
| `diag_owner__needless_move_warning` | move in a context where it changes nothing | Warning makes optimization/readability point without blocking compilation | Quality diagnostics should remain stable once introduced |

## Edge Cases

- Multiple related notes: move site plus invalid use site
- Ownership diagnostics inside generic or overloaded calls
- Diagnostics on field/element moves, not just plain locals
- Warning vs error boundary for suspicious patterns

## Happy Path

Happy-path tests here ensure that legal alias/copy/move patterns emit no ownership diagnostics.

## Error Path

Error-path tests should pin both message intent and note structure whenever secondary locations are present.
