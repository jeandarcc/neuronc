# Semantic Behavior Specification

## Scope

This folder covers meaning-level validation of `.nr` programs after successful parse. It specifies what source programs are well-typed, well-resolved, and semantically executable from the user’s perspective.

Subfolders:

- [type_checking/README.md](type_checking/README.md)
- [type_inference/README.md](type_inference/README.md)
- [scope/README.md](scope/README.md)
- [generics/README.md](generics/README.md)
- [overloading/README.md](overloading/README.md)

Included:

- Type compatibility
- Name resolution and duplicate detection
- Generic instantiation rules
- Overload resolution
- Semantic warnings that are visible and stable enough for end users

Excluded:

- Parser-only malformed syntax
- Internal optimizer legality
- LSP symbol behavior

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N2000-N2099` | type mismatches and invalid operator use |
| `N2100-N2199` | inference failures or ambiguity |
| `N2200-N2299` | scope, symbol, and visibility failures |
| `N2300-N2399` | generic arity, substitution, and constraint failures |
| `N2400-N2499` | overload resolution failures |
| `W2000-W2099` | semantic warnings with stable user value |
| `NR2001` | current coarse semantic fallback |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `semantic_baseline__valid_arithmetic_types` | `Init is method() { x is 1 + 2; Print(x); }` | Compiles and prints `3` | Baseline semantic success |
| `semantic_baseline__invalid_addition_pair` | `Init is method() { x is "a" + 1; }` | `N2000-N2099` or `NR2001` | Distinguishes semantic rejection from parse rejection |
| `semantic_baseline__unknown_identifier` | `Init is method() { Print(missing); }` | `N2200-N2299` | Keeps name resolution strict |
| `semantic_baseline__inference_from_literal` | `Init is method() { x is 42; Print(x); }` | Compiles and prints `42` | Pins obvious inference |
| `semantic_baseline__generic_arity_failure` | `value as Array<int, string>;` | `N2300-N2399` | Prevents silent acceptance of malformed generic use |
| `semantic_baseline__ambiguous_overload` | overloaded call with equal candidates | `N2400-N2499` | Ensures ambiguity remains diagnosable |
| `semantic_baseline__warning_only_shadow` | legal-but-risky shadowing program | Warning in `W2000-W2099` and successful compilation | Confirms warning-only outcomes are representable |

## Edge Cases

- Semantic success after parser recovery from unrelated earlier syntax noise
- Duplicate names across local, member, and module scope
- Generic substitution that changes operator legality
- Overload resolution mixed with implicit inference
- Branch-local declarations that should or should not escape
- Built-in names such as `Print` and `Input` used as values

## Happy Path

Happy-path semantic tests compile cleanly and then defer runtime assertions to the relevant `codegen/`, `ownership/`, or `stdlib/` folders.

## Error Path

Error-path semantic tests must specify:

- the exact construct being rejected
- the expected diagnostic code/range
- the candidate source span
- whether analysis continues to collect additional semantic errors
