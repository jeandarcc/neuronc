# Ownership Borrowing and Aliasing Specification

## Scope

This folder covers non-owning access patterns: alias-by-default `is`, pointer-style `address of` / `value of`, writes through aliases, and invalid aliasing patterns if the language restricts them.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N3100-N3199` | invalid borrow/alias behavior |
| `N3200-N3299` | alias escape that becomes a lifetime violation |
| `W3000-W3099` | suspicious alias chains that remain legal |
| `NPP2001` | current semantic fallback |

## Test List

| Name | `.npp` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `borrow__alias_by_default` | `Init is method() { x is 10; y is x; y is 20; Print(x); }` | Compiles and prints `20` | Defines default alias semantics |
| `borrow__copy_is_not_alias` | `Init is method() { x is 10; y is another x; y is 20; Print(x); }` | Compiles and prints `10` | Separates copy behavior from borrow behavior |
| `borrow__address_and_dereference_roundtrip` | `Init is method() { x is 42; p is address of x; Print(value of p); }` | Compiles and prints `42` | Baseline pointer borrow behavior |
| `borrow__write_through_pointer` | `Init is method() { x is 42; p is address of x; value of p is 99; Print(x); }` | Compiles and prints `99` | Makes mutation through borrowed access observable |
| `borrow__alias_after_owner_move` | `Init is method() { x is 10; p is address of x; y is move x; Print(value of p); }` | Either `N3100-N3199`/`N3200-N3299` or a clearly documented valid behavior; tests must lock the rule | Critical interaction between aliasing and moves |
| `borrow__escaping_pointer_to_inner_scope` | inner-scope local addressed and used outside that scope | `N3200-N3299` | Prevents dangling borrowed access |

## Edge Cases

- Borrow chains such as pointer-to-pointer
- Alias mutation inside loops and conditionals
- Borrow from struct fields and collection elements
- Borrowing `const` values and whether mutation through borrow is forbidden
- Mixed `is`, `another`, `move`, and pointer operations in one block

## Happy Path

Happy-path tests should make aliasing visible through changed output, not merely through successful compilation.

## Error Path

Error-path tests should distinguish an illegal alias itself from a later illegal use through that alias.
