# Parser Expressions Specification

## Scope

This folder specifies expression grammar: precedence, associativity, unary forms, calls, member access, indexing, slicing, pointer forms, and grouping.

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N1100-N1199` | malformed expression grammar |
| `N1000-N1099` | token-level issues inside expressions |
| `NPP1002` | current coarse parser fallback |

## Test List

| Name | `.npp` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `expr__precedence_mul_over_add` | `value is 1 + 2 * 3;` | Parse succeeds with `*` tighter than `+` | Prevents precedence drift |
| `expr__parenthesized_grouping` | `value is (1 + 2) * 3;` | Parse succeeds with explicit grouping | Confirms parentheses override precedence |
| `expr__function_call` | `value is Add(1, 2);` | Parse succeeds | Baseline call grammar |
| `expr__member_and_call_chain` | `length is vector.Normalize().Length;` | Parse succeeds | Covers chained postfix parsing |
| `expr__index_and_slice` | `item is values[0]; part is values[0..2];` | Parse succeeds | Keeps indexing and slice forms stable |
| `expr__address_and_value_of` | `p is address of x; y is value of p;` | Parse succeeds | Pins readable pointer grammar |
| `expr__trailing_operator` | `value is 1 + ;` | `N1100-N1199` | Classic incomplete-expression failure |
| `expr__missing_closing_paren` | `value is Add((1 + 2);` | `N1100-N1199` or `N1500-N1599` | Ensures delimiter recovery stays useful |

## Edge Cases

- Unary minus next to member access or function calls
- Nested indexing and slicing such as `matrix[0][1]`
- `typeof(...)` and cast-like forms if they parse as expressions
- Call arguments containing lambdas, generics, or nested conditionals
- Postfix chains ending at end of line vs before semicolon
- Ambiguous-looking whitespace around `address of` and `value of`

## Happy Path

Happy-path tests should cover each postfix form alone and in chains:

- call
- member access
- index
- slice
- pointer dereference

## Error Path

Error-path tests should isolate whether failure comes from:

- missing operand
- missing delimiter
- malformed postfix continuation
- illegal token after an otherwise complete expression
