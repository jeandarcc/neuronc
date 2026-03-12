# Parser Literals Specification

## Scope

This folder specifies how standalone literal forms are parsed in `.npp` source before semantic meaning is considered.

Covered literal families:

- integer literals
- float literals
- string literals and escapes
- boolean literals
- literal use inside bindings, returns, calls, and collections/tensors where syntax alone is the subject

## Error Codes

| Code or range | Meaning |
| --- | --- |
| `N1000-N1099` | malformed literal token forms |
| `N1100-N1199` | literal used in malformed surrounding expression syntax |
| `W1000-W1099` | tolerated but suspicious literal spellings, if any compatibility mode exists |
| `NPP1001`, `NPP1002` | current lexer/parser fallbacks |

## Test List

| Name | `.npp` input code | Expected output or error | Why important |
| --- | --- | --- | --- |
| `literal__integer_decimal` | `x is 42;` | Parse succeeds | Smallest numeric literal baseline |
| `literal__float_decimal` | `x is 3.14;` | Parse succeeds | Pins decimal float syntax |
| `literal__boolean_true_false` | `a is true; b is false;` | Parse succeeds | Confirms keyword literals stay reserved and parseable |
| `literal__string_plain` | `name is "Neuron";` | Parse succeeds | Baseline string token coverage |
| `literal__string_escape_quote` | `quote is "say \"hi\"";` | Parse succeeds | Keeps common escaping stable |
| `literal__negative_number_is_unary_expr` | `x is -1;` | Parse succeeds as unary-minus plus literal, not a special token form | Avoids precedence regressions |
| `literal__unterminated_string` | `name is "Neuron;` | `N1000-N1099` or lexer fallback | Common user typo with high diagnostic value |
| `literal__bad_escape_sequence` | `path is "a\q";` | `N1000-N1099` | Ensures invalid escapes are rejected early |

## Edge Cases

- Empty string literal `""`
- Very long literal text on one line
- Literals adjacent to comments
- Float literals near trailing dots or multiple dots
- Leading zero integers if the language treats them specially
- Literal at end of file with no trailing newline
- Literal immediately followed by delimiter characters such as `)`, `]`, `,`, `;`

## Happy Path

Happy-path tests should show each literal family embedded in at least:

- a binding
- a return statement
- a function-call argument

## Error Path

Error-path tests should distinguish malformed token spelling from otherwise valid literals placed in malformed expression contexts.
