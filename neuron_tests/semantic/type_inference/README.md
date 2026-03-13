# Semantic Type Inference Specification

## Scope

This folder covers cases where the compiler deduces types from literals, expressions, calls, and generic instantiations.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N2100-N2199` | inference ambiguity, conflicting branch inference, or missing context |
| `N2000-N2099` | downstream mismatch after an inferred type is established |
| `W2000-W2099` | suspicious but accepted inference outcomes |
| `NR2001` | current coarse semantic fallback |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `infer__literal_int` | `Init is method() { x is 42; Print(x); }` | Compiles and prints `42`; inferred type is `int` | Baseline literal inference |
| `infer__literal_float` | `Init is method() { x is 3.14; Print(x); }` | Compiles and prints normalized float output | Confirms float inference stays distinct from `int` |
| `infer__expression_result` | `Init is method() { a is 5; b is a * 2; Print(b); }` | Compiles and prints `10` | Ensures expression-based inference composes |
| `infer__generic_identity` | `Identity method<T>(value as T) as T { return value; } Init is method() { result is Identity("hello"); Print(result); }` | Compiles and prints `hello` | Core generic-return inference scenario |
| `infer__input_defaults_to_string` | `Init is method() { value is Input("Enter: ").Secret().Default("guest"); }` | Semantic success | Mirrors an existing built-in inference rule |
| `infer__null_without_context` | `Init is method() { value is null; }` | Either a defined inferred nullable/dynamic type or `N2100-N2199`; README-driven tests must lock the chosen language rule | Forces the null-inference contract to be explicit |
| `infer__conflicting_branch_inference` | `Init is method(flag as bool) { value is 1; if(flag) { value is "x"; } }` | `N2100-N2199` or a precisely defined dynamic-widening rule | Prevents accidental widening regressions |

## Edge Cases

- Inference from `another` and `move`
- Inference through named arguments and overloaded calls
- Inference from `typeof(...)`
- Inference for collection literals if/when they exist
- Rebinding dynamic variables after a previous inferred type
- Branch-local inference that should not leak outside the branch

## Happy Path

Happy-path tests should prove the compiler infers the same type regardless of harmless formatting changes, nested expressions, or call indirection.

## Error Path

Error-path tests should define whether ambiguity is diagnosed at the binding site, the first conflicting assignment, or the later use site.
