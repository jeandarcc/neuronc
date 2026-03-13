# Semantic Diagnostic Specification

## Scope

This folder covers message quality for semantic errors and warnings unrelated to ownership or modules.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N2000-N2099` | direct type mismatch and invalid operation diagnostics |
| `N2100-N2199` | inference ambiguity diagnostics |
| `N2200-N2299` | scope and symbol diagnostics |
| `N2300-N2399` | generic diagnostics excluding ownership-specific cases |
| `N2400-N2499` | overload diagnostics |
| `W2000-W2099` | semantic warnings |
| `NR2001` | current fallback |

## Test List

| Name | `.nr` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `diag_semantic__string_plus_int` | `Init is method() { value is "a" + 1; }` | Message names incompatible operand types | Type mismatch diagnostics should teach, not merely reject |
| `diag_semantic__unknown_identifier` | `Init is method() { Print(missing); }` | Message includes unresolved name `missing` | Identifier text must survive formatting changes |
| `diag_semantic__builtin_name_hint` | `Print;` | Message suggests calling `Print(...)` instead of using it as a value | Existing repo behavior worth preserving exactly |
| `diag_semantic__unknown_named_argument` | `Sum(third: 2, first: 1)` against `(first, second)` | Message includes bad label `third` | Named-argument diagnostics need precision |
| `diag_semantic__unreachable_code_warning` | `return; Print("dead");` | Warning severity plus stable â€œUnreachable codeâ€ wording | Severity correctness matters as much as content |

## Edge Cases

- Diagnostics mentioning both expected and actual types
- Multiple semantic errors in one function
- Generic substitution errors that mention instantiated types
- Warnings with no compile failure
- Diagnostics after parser recovery

## Happy Path

Happy-path tests assert that semantically valid programs stay silent even when they use advanced features such as named arguments or generics.

## Error Path

Error-path tests should require the diagnostic to identify the user-written construct, not only the compilerâ€™s internal interpretation.
