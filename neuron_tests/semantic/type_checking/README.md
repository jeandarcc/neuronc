# Semantic Type Checking Specification

## Scope

This folder covers source programs that parse correctly but must be accepted or rejected based on type compatibility rules.

Covered rules:

- assignment compatibility
- return-type compatibility
- argument compatibility
- operator operand legality
- condition expression legality
- cast validity
- const mutation rules when they manifest as semantic rejection

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N2000-N2099` | direct type mismatch or invalid operator pair |
| `N2100-N2199` | type-related ambiguity caused by insufficient context |
| `W2000-W2099` | warning-only type quality issues such as unreachable code or redundant casts |
| `NR2001` | current coarse semantic fallback |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `typecheck__valid_integer_arithmetic` | `Init is method() { value is 1 + 2; Print(value); }` | Compiles and prints `3` | Baseline successful typing |
| `typecheck__invalid_string_plus_int` | `Init is method() { value is "a" + 1; }` | `N2000-N2099` | Classic incompatible operator test |
| `typecheck__return_type_mismatch` | `Answer method() as int { return "bad"; }` | `N2000-N2099` | Prevents silent coercion of returns |
| `typecheck__named_argument_unknown` | `Sum is method(first as int, second as int) as int { return first + second; } Init is method() { Sum(third: 2, first: 1); }` | `N2000-N2099` or `N2200-N2299` | Keeps named-argument checking explicit |
| `typecheck__invalid_cast_step` | `Init method() { flag is true as bool; flag as Tensor<float>; }` | `N2000-N2099` | Mirrors an existing semantic rule in the repo |
| `typecheck__const_mutation_rejected` | `Init is method() { const count is 2 as int; count--; }` | `N2000-N2099` | Treats constness as part of source semantics |
| `typecheck__unreachable_code_warning` | `Init is method() { return; Print("dead"); }` | Warning in `W2000-W2099`, compilation behavior explicitly defined | Ensures warning-only semantic contracts are testable |

## Edge Cases

- Mixed numeric types such as `int` with `float`
- Valid vs invalid casts between dynamic, string, numeric, tensor, and enum types
- Function calls whose argument mismatch appears only after named-argument reordering
- Mutation through aliases or dereference expressions
- Condition expressions that are non-boolean if the language forbids truthiness coercion
- Return statements inside nested control flow where some paths mismatch

## Happy Path

Happy-path tests should cover valid operator, call, assignment, return, and cast combinations across primitives, generics, and user-defined types.

## Error Path

Error-path tests should pinpoint both source and destination types whenever the compiler exposes them in diagnostics.
