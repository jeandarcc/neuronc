# Ownership Lifetime Specification

## Scope

This folder covers whether references, aliases, pointers, and moved values remain valid across scope boundaries, returns, branches, and stored state.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N3200-N3299` | dangling reference/alias/pointer, invalid scope escape, expired value use |
| `N3000-N3099` | lifetime issues rooted in prior move invalidation |
| `NPP2001` | current semantic fallback |

## Test List

| Name | `.npp` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `lifetime__return_local_address_rejected` | function returns `address of` a local variable | `N3200-N3299` | Canonical dangling-reference scenario |
| `lifetime__inner_scope_alias_escape` | alias or pointer to inner-scope local used after block exit | `N3200-N3299` | Baseline scope-escape rejection |
| `lifetime__valid_return_of_owned_copy` | helper returns `another` copy of an input or local value | Compiles and returns expected value | Contrasts safe ownership transfer with invalid borrow escape |
| `lifetime__branch_maybe_null_use` | `value` becomes `null` in one branch then member-accessed later | `N3200-N3299` or `N2000-N2099` depending on implementation | Existing semantic behavior tied to value validity over time |
| `lifetime__maybe_uninitialized_after_branch` | variable assigned in only one branch then used | `N3200-N3299` or semantic fallback | Another time-dependent validity rule already present in semantic tests |
| `lifetime__capture_after_move` | value moved, then referenced through a previously established alias | `N3200-N3299` | Prevents stale references to moved storage |

## Edge Cases

- Lifetime across loop iterations
- Lifetime across `try/catch/finally`
- Returning aliases to parameters vs locals
- Storing borrowed values in structs or globals
- Lifetime of dereferenced pointers created from nested `address of`

## Happy Path

Happy-path tests should prove safe lifetimes with observable outputs after scope transitions, returns, or branch joins.

## Error Path

Error-path tests should define whether the primary diagnostic appears at the escape point, the later use point, or both.
